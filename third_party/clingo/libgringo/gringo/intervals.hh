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

#ifndef GRINGO_INTERVALS_HH
#define GRINGO_INTERVALS_HH

#include <vector>
#include <iostream>
#include <algorithm>

namespace Gringo {

// NOTE: refactor this is almost the same as interval_set ...

template <class T>
class IntervalSet {
public:
    // Notes: read < as before with gap (except if bounds of same type are compared)
    using Value = T;
    struct RBound;

    struct LBound {
        LBound &operator=(RBound const &x) {
            bound     = x.bound;
            inclusive = !x.inclusive;
            return *this;
        }
        bool operator<(LBound const &x) const {
            return bound < x.bound || (!(x.bound < bound) &&  inclusive && !x.inclusive );
        }
        bool operator<=(LBound const &x) const {
            return bound < x.bound || (!(x.bound < bound) && (inclusive || !x.inclusive));
        }
        bool operator<(RBound const &x) const {
            return bound < x.bound || (!(x.bound < bound) &&  inclusive &&  x.inclusive );
        }
        Value bound;
        bool inclusive;
    };

    struct RBound {
        RBound &operator=(LBound const &x) {
            bound     = x.bound;
            inclusive = !x.inclusive;
            return *this;
        }
        bool operator<(RBound const &x) const {
            return bound < x.bound || (!(x.bound < bound) &&  !inclusive &&  x.inclusive );
        }
        bool operator<=(RBound const &x) const {
            return bound < x.bound || (!(x.bound < bound) && (!inclusive ||  x.inclusive));
        }
        bool operator<(LBound const &x) const {
            return bound < x.bound || (!(x.bound < bound) &&  !inclusive && !x.inclusive );
        }
        Value bound;
        bool inclusive;
    };

    struct Interval {
        bool contains(T const &x) const {
            return !(x < *this) && !(*this < x);
        }
        bool empty() const {
            return !(left < right);
        }
        bool operator<(Interval const &x) const {
            return right < x.left;
        }
        LBound left;
        RBound right;
    };
    using IntervalVec = std::vector<Interval>;
    using const_iterator = typename IntervalVec::const_iterator;

    IntervalSet(std::initializer_list<Interval> list) {
        for (auto &x : list) {
            add(x);
        }
    }

    IntervalSet(Interval const &x) {
        add(x);
    }

    IntervalSet(Value const &a, bool ta, Value const &b, bool tb) {
        add({{a, ta}, {b, tb}});
    }

    IntervalSet(IntervalSet &&other) noexcept = default;
    IntervalSet(IntervalSet const &other) = default;
    IntervalSet() = default;
    IntervalSet &operator=(IntervalSet const &other) = default;
    IntervalSet &operator=(IntervalSet &&other) noexcept = default;
    ~IntervalSet() noexcept = default;

    void add(Interval const &x) {
        if (!x.empty()) {
            auto it = std::lower_bound(vec_.begin(), vec_.end(), x);
            if (it == vec_.end()) {
                vec_.emplace_back(x);
            }
            else {
                auto jt = std::upper_bound(it, vec_.end(), x);
                if (it == jt) {
                    vec_.emplace(it, x);
                }
                else {
                    it->left  = std::min(x.left, it->left);
                    it->right = std::max(x.right, (jt-1)->right);
                    vec_.erase(it+1, jt);
                }
            }
        }
    }

    void remove(Interval const &x) {
        if (!x.empty()) {
            auto it = std::lower_bound(vec_.begin(), vec_.end(), x);
            if (it != vec_.end()) {
                auto jt = std::upper_bound(it, vec_.end(), x);
                if (it+1 == jt) {
                    Interval r;
                    r.left    = x.right;
                    r.right   = it->right;
                    it->right = x.left;
                    if (it->empty()) {
                        if (r.empty()) {
                            vec_.erase(it);
                        }
                        else {
                            *it = r;
                        }
                    }
                    else if (!r.empty()) {
                        vec_.emplace(it+1, r);
                    }
                }
                else if (it != jt) {
                    it->right    = x.left;
                    (jt-1)->left = x.right;
                    vec_.erase(it + !it->empty(), jt - !(jt-1)->empty());
                }
            }
        }
    }

    bool contains(Interval const &x) const {
        if (x.empty()) {
            return true;
        }
        for (auto &y : vec_) {
            if (x.right <= y.right) {
                return y.left <= x.left;
            }
        }
        return false;
    }

    bool intersects(Interval const &x) const {
        if (!x.empty()) {
            for (auto &y : vec_) {
                if (x.left < y.right) {
                    return y.left < x.right;
                }
            }
        }
        return false;
    }

    bool empty() const {
        return vec_.empty();
    }

    size_t size() const {
        return vec_.size();
    }

    Interval const &front() const {
        return vec_.front();
    }

    Interval const &back() const {
        return vec_.back();
    }

    void clear() {
        return vec_.clear();
    }

    void add(Value const &a, bool ta, Value const &b, bool tb) {
        add({{a, ta}, {b, tb}});
    }

    const_iterator begin() const {
        return vec_.begin();
    }

    const_iterator end() const {
        return vec_.end();
    }

    void remove(Value const &a, bool ta, Value const &b, bool tb) {
        return remove({{a, ta}, {b, tb}});
    }

    IntervalSet intersect(IntervalSet const &set) const {
        auto it = vec_.begin();
        IntervalSet intersection;
        for (auto &x : set.vec_) {
            for (; it != vec_.end() && it->right < x.left; ++it) { }
            for (; it != vec_.end() && it->right <= x.right; ++it) {
                intersection.vec_.emplace_back(Interval{std::max(it->left, x.left), it->right});
            }
            if (it != vec_.end() && it->left < x.right) {
                intersection.vec_.emplace_back(Interval{std::max(it->left, x.left), x.right});
            }
        }
        return intersection;
    }

    IntervalSet difference(IntervalSet const &set) const {
        auto it = set.vec_.begin();
        IntervalSet difference;
        for (auto &x : vec_) {
            Interval current = x;
            for (; it != set.vec_.end() && it->right < current.left; ++it) { }
            for (; it != set.vec_.end() && it->right <= current.right; ++it) {
                if (current.left < it->left) {
                    difference.vec_.emplace_back(current);
                    difference.vec_.back().right = it->left;
                }
                current.left = it->right;
            }
            if (it != set.vec_.end() && it->left < current.right) {
                current.right = it->left;
            }
            if (current.left < current.right) {
                difference.vec_.emplace_back(current);
            }
        }
        return difference;
    }

private:
    IntervalVec vec_;
};

template <class T>
bool operator<(T const &a, typename IntervalSet<T>::Interval const &b) {
    return a < b.left.bound || (!(b.left.bound < a) && !b.left.inclusive);
}

template <class T>
bool operator<(typename IntervalSet<T>::Interval const &a, T const &b) {
    return a.right.bound < b || (!(b < a.right.bound) && !a.right.inclusive);
}

template <class T>
typename Gringo::IntervalSet<T>::const_iterator begin(IntervalSet<T> const &x) {
    return x.begin();
}

template <class T>
typename Gringo::IntervalSet<T>::const_iterator end(IntervalSet<T> const &x) {
    return x.end();
}

} // namespace Gringo

#endif // GRINGO_INTERVALS_HH
