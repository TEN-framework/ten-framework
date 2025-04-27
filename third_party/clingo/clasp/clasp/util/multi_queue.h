//
// Copyright (c) 2010-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
#ifndef CLASP_MULIT_QUEUE_H_INCLUDED
#define CLASP_MULIT_QUEUE_H_INCLUDED

#include <clasp/config.h>

namespace Clasp { namespace mt { namespace Detail {

struct RawNode;
typedef Clasp::Atomic_t<RawNode*>::type SafeNodePtr;
struct RawNode {
	SafeNodePtr next;
};
// Lock-free stack that is NOT ABA-safe by itself
struct RawStack {
	RawStack() { top = static_cast<RawNode*>(0); }
	RawNode* tryPop() {
		RawNode* n = 0, *next = 0;
		do {
			if ((n = top) == 0) { return 0; }
			// NOTE:
			// it is the caller's job to guarantee that n is safe
			// and n->next is ABA-safe at this point.
			next = n->next;
		} while (compare_and_swap(top, n, next) != n);
		return n;
	}
	void     push(RawNode* n) {
		RawNode* assumedTop;
		do {
			assumedTop = top;
			n->next    = assumedTop;
		} while (compare_and_swap(top, assumedTop, n) != assumedTop);
	}
	SafeNodePtr top;
};
struct DefaultDeleter {
	template <class T>
	void operator()(T& obj) const {
		(void)obj;
		obj.~T();
	}
};
}

//! A (base) class for distributing items between n different threads.
/*!
 * Logically, the class maintains n queues, one for each
 * involved thread. Threads must register themselves by
 * calling addThread(). The returned handle has then
 * to be used for publishing and consuming items.
 */
template <class T, class Deleter = Detail::DefaultDeleter>
class MultiQueue {
protected:
	typedef Detail::RawNode RawNode;
	typedef Clasp::Atomic_t<int>::type SafeInt;
	struct Node : Detail::RawNode {
		explicit Node(uint32 rc, const T& d) : data(d) { next = 0; refs = rc; }
		SafeInt refs;
		T       data;
	};
public:
	typedef Detail::RawNode* ThreadId;
	//! creates a new object for at most m threads
	explicit MultiQueue(uint32 m, const Deleter& d = Deleter()) : maxQ_(m), deleter_(d) {
		head_.next = 0;
		tail_      = &head_;
	}
	uint32 maxThreads() const { return maxQ_; }
	void reserve(uint32 c) {
		struct NodeHead : RawNode { SafeInt refs; };
		for (uint32 i = 0; i != c; ++i) {
			free_.push(new (::operator new(sizeof(Node))) NodeHead());
		}
	}
	//! destroys the object and all unconsumed items
	~MultiQueue() {
		for (Detail::RawNode* x = head_.next; x ; ) {
			Node* n = toNode(x);
			x = x->next;
			deleter_(n->data);
			::operator delete(n);
		}
		for (Detail::RawNode* x; (x = free_.tryPop()) != 0; ) {
			::operator delete(toNode(x));
		}
	}
	//! adds a new thread to the object
	/*!
	 * \note Shall be called at most m times
	 * \return A handle identifying the new thread
	 */
	ThreadId addThread() {
		return &head_;
	}
	bool hasItems(ThreadId& cId) const { return cId != tail_; }

	//! tries to consume an item
	/*!
	 * \pre cId was initially obtained via a call to addThread()
	 * \note tryConsume() is thread-safe w.r.t different ThreadIds
	 */
	bool tryConsume(ThreadId& cId, T& out) {
		if (cId != tail_) {
			RawNode* n = cId;
			cId        = cId->next;
			assert(cId != 0 && "MultiQueue is corrupted!");
			release(n);
			out = toNode(cId)->data;
			return true;
		}
		return false;
	}
	//! pops an item from the queue associated with the given thread
	/*!
	 * \pre hasItems(cId) == true
	 */
	void pop(ThreadId& cId) {
		assert(hasItems(cId) && "Cannot pop from empty queue!");
		RawNode* n = cId;
		cId        = cId->next;
		release(n);
	}
protected:
	//! publishes a new item
	/*!
	 * \note the function is *not* thread-safe, i.e.
	 * it must not be called concurrently
	 */
	void unsafePublish(const T& in, const ThreadId&) { unsafePublish(in); }
	void unsafePublish(const T& in) { publishRelaxed(allocate(maxQ_, in)); }

	//! concurrency-safe version of unsafePublish
	void publish(const T& in, const ThreadId&) {
		RawNode* newNode = allocate(maxQ_, in);
		RawNode* assumedTail, *assumedNext;
		do {
			assumedTail = tail_;
			assumedNext = assumedTail->next;
			if (assumedTail != tail_) {
				// tail has changed - try again
				continue;
			}
			if (assumedNext != 0) {
				// someone has added a new node but has not yet
				// moved the tail - assist him and start over
				compare_and_swap(tail_, assumedTail, assumedNext);
				continue;
			}
		} while (compare_and_swap(assumedTail->next, static_cast<RawNode*>(0), newNode) != 0);
		// Now that we managed to link a new node to what we think is the current tail
		// we try to update the tail. If the tail is still what we think it is,
		// it is moved - otherwise some other thread already did that for us.
		compare_and_swap(tail_, assumedTail, newNode);
	}

	//! Non-atomically adds n to the global queue
	void publishRelaxed(Node* n) {
		static_cast<RawNode*>(tail_)->next = n;
		tail_ = n;
	}
	uint32 maxQ() const { return maxQ_; }
	Node*  allocate(uint32 maxR, const T& in) {
		// If the queue is used correctly, the raw stack is ABA-safe at this point.
		// The ref-counting in the queue makes sure that a node cannot be added back
		// to the stack while another thread is still in tryPop() - that thread had
		// not yet the chance to decrease the node's ref count.
		if (Node* n = toNode(free_.tryPop())) {
			n->next = 0;
			n->refs = maxR;
			new (&n->data)T(in);
			return n;
		}
		return new (::operator new(sizeof(Node))) Node(maxR, in);
	}
private:
	MultiQueue(const MultiQueue&);
	MultiQueue& operator=(const MultiQueue&);
	Node*toNode(Detail::RawNode* x) const { return static_cast<Node*>(x); }
	void release(Detail::RawNode* n) {
		if (n != &head_ && --toNode(n)->refs == 0) {
			head_.next = static_cast<RawNode*>(n->next);
			deleter_(toNode(n)->data);
			free_.push(n);
		}
	}
	RawNode             head_;
	Detail::SafeNodePtr tail_;
	Detail::RawStack    free_;
	const uint32        maxQ_;
	Deleter             deleter_;
};

//! Unbounded non-intrusive lock-free multi-producer single consumer queue.
/*!
 * Based on Dmitriy Vyukov's MPSC queue:
 * http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
 */
class MPSCPtrQueue {
public:
	typedef Detail::RawNode RawNode;
	struct Node : RawNode { void* data; };
	typedef Clasp::Atomic_t<Node*>::type SafeNodePtr;
	Node* toNode(RawNode* n) const { return static_cast<Node*>(n); }
	MPSCPtrQueue() {}
	void init(Node* sent) {
		sent->next = 0;
		sent->data = 0;
		head_      = sent;
		tail_      = sent;
	}
	bool empty() const { return !static_cast<RawNode*>(tail_->next); }
	void push(Node* n) {
		n->next = 0;
		Node* p = head_.exchange(n);
		p->next = n;
	}
	Node* pop() {
		Node* t = tail_;
		Node* n = toNode(t->next);
		if (!n) { return 0; }
		tail_   = n;
		t->data = n->data;
		n->data = 0;
		return t;
	}
private:
	MPSCPtrQueue(const MPSCPtrQueue&);
	MPSCPtrQueue& operator=(const MPSCPtrQueue&);
	SafeNodePtr head_; // producers
	char        pad_[64 - sizeof(Node*)];
	Node*       tail_; // consumer
};

} } // end namespace Clasp::mt
#endif
