// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#include <gringo/ground/instantiation.hh>
#include <gringo/output/output.hh>

#define DEBUG_INSTANTIATION 0

namespace Gringo { namespace Ground {

// {{{ definition of SolutionBinder

IndexUpdater *SolutionBinder::getUpdater() {
    return nullptr;
}

void SolutionBinder::match(Logger &log) {
    static_cast<void>(log);
}

bool SolutionBinder::next() {
    return false;
}

void SolutionBinder::print(std::ostream &out) const {
    out << "#end";
}

// }}}
// {{{ definition of BackjumpBinder

BackjumpBinder::BackjumpBinder(UIdx &&index, DependVec &&depends)
: index(std::move(index))
, depends(std::move(depends)) { }

void BackjumpBinder::match(Logger &log) const {
    index->match(log);
}

bool BackjumpBinder::next() const {
    return index->next();
}

bool BackjumpBinder::first(Logger &log) const {
    index->match(log);
    return next();
}

void BackjumpBinder::print(std::ostream &out) const {
    out << *index;
    out << ":[";
    print_comma(out, depends, ",");
    out << "]";
}

// }}}
// {{{ definition of Instantiator

Instantiator::Instantiator(SolutionCallback &callback)
: callback(&callback) { }

void Instantiator::add(UIdx &&index, DependVec &&depends) {
    binders.emplace_back(std::move(index), std::move(depends));
}

void Instantiator::finalize(DependVec &&depends) {
    binders.emplace_back(gringo_make_unique<SolutionBinder>(), std::move(depends));
}

void Instantiator::enqueue(Queue &queue) { queue.enqueue(*this); }

void Instantiator::instantiate(Output::OutputBase &out, Logger &log) {
#if DEBUG_INSTANTIATION > 0
    std::cerr << "  instantiate: " << *this << std::endl;
#endif
    auto ie = binders.rend();
    auto it = ie - 1;
    auto ib = binders.rbegin();
    it->match(log);
    do {
#if DEBUG_INSTANTIATION > 1
        std::cerr << "    start at: " << *it << std::endl;
#endif
        it->backjumpable = true;
        if (it->next()) {
            for (--it; it->first(log); --it) { it->backjumpable = true; }
#if DEBUG_INSTANTIATION > 1
            std::cerr << "    advanced to: " << *it << std::endl;
#endif
        }
        if (it == ib) { callback->report(out, log); }
        for (auto &x : it->depends) { binders[x].backjumpable = false; }
        for (++it; it != ie && it->backjumpable; ++it) { }
#if DEBUG_INSTANTIATION > 1
        std::cerr << "    backfumped to: ";
        if (it != ie) { it->print(std::cerr); }
        else          { std::cerr << "the head :)"; }
        std::cerr << std::endl;
#endif
    }
    while (it != ie);
}

void Instantiator::print(std::ostream &out) const {
    using namespace std::placeholders;
    // Note: consider adding something to callback
    callback->printHead(out);
    out << " <~ ";
    print_comma(out, binders, " , ", [](std::ostream &out, BackjumpBinder const &x) { x.print(out); });
    out << ".";
}

unsigned Instantiator::priority() const {
    return callback->priority();
}

// }}}
// {{{ definition of Queue

void Queue::process(Output::OutputBase &out, Logger &log) {
    bool empty = true;
    do {
        empty = true;
        for (auto &queue : queues) {
            if (!queue.empty()) {
#if DEBUG_INSTANTIATION > 0
                std::cerr << "************start step" << std::endl;
#endif
                queue.swap(current);
                for (Instantiator &x : current) {
                    x.instantiate(out, log);
                    x.enqueued = false;
                }
                for (Instantiator &x : current) { x.callback->propagate(*this); }
                current.clear();
                // OPEN -> NEW, NEW -> OLD
                auto jt = std::remove_if(domains.begin(), domains.end(), [](Domain &x) -> bool {
                    x.nextGeneration();
                    return !x.dequeue();
                });
                domains.erase(jt, domains.end());
                empty = false;
                break;
            }
        }
    }
    while (!empty);
    // NEW -> OLD
    for (Domain &x : domains) {
        x.nextGeneration();
        if (x.dequeue()) { assert(false); }
    }
    domains.clear();
}
void Queue::enqueue(Instantiator &inst) {
    if (!inst.enqueued) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        queues[inst.priority()].emplace_back(inst);
        inst.enqueued = true;
    }
}
void Queue::enqueue(Domain &dom) {
    if (!dom.isEnqueued()) {
        domains.emplace_back(dom);
    }
    dom.enqueue();
}

// }}}

} } // namespace Ground Gringo
