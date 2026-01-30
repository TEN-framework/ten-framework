##
## Set Test
##

assert("Set.new") do
  assert_nothing_raised {
    Set.new()
    Set.new(nil)
    Set.new([])
    Set.new([1,2])
    Set.new(1..3)
  }
  assert_raise(ArgumentError) { Set.new(false) }
  assert_raise(ArgumentError) { Set.new(1) }
  assert_raise(ArgumentError) { Set.new(1,2) }

  ary = [2,4,6,4]
  set = Set.new(ary)
  ary.clear
  assert_false set.empty?
  assert_equal(3, set.size)

  ary = [1,2,3]

  s = Set.new(ary) { |o| o * 2 }
  assert_equal([2,4,6], s.sort)
end

assert("Set.[]") do
  assert_nothing_raised {
    Set[]
    Set[nil]
    Set[[]]
    Set[[1,2]]
    Set['a'..'c']
    Set[false]
    Set[1]
    Set[1,2]
  }

  ary = [2,4,6,4]
  set = Set[ary]
  ary.clear
  assert_false set.empty?
  assert_equal([[]], set.to_a)
end

assert("Set#clone") do
  set1 = Set.new
  set2 = set1.clone

  assert_false set1.equal?(set2) # assert_not_same

  assert_equal(set1, set2)

  set1 << 'abc'

  assert_equal(Set.new, set2)
end

assert("Set#dup") do
  set1 = Set[1,2]
  set2 = set1.dup

  assert_false set1.equal?(set2) # assert_not_same

  assert_equal(set1, set2)

  set1 << 'abc'

  assert_equal(Set[1,2], set2)
end

assert("Set#size") do
  assert_equal(0, Set[].size)
  assert_equal(1, Set[nil].size)
  assert_equal(1, Set[[]].size)
  assert_equal(1, Set[[nil]].size)
end

assert("Set#empty?") do
  assert_true Set[].empty?
  assert_false Set[1,2].empty?
end

assert("Set#clear") do
  set = Set[1,2]
  ret = set.clear

  assert_true set.equal?(ret) # assert_same
  assert_true set.empty?
end

assert("Set#replace") do
  set = Set[1,2]
  ret = set.replace(['a','b','c'])

  assert_true set.equal?(ret) # assert_same
  assert_equal(Set['a','b','c'], set)


  set = Set[1,2]
  ret = set.replace(Set['a','b','c'])

  assert_true set.equal?(ret) # assert_same
  assert_equal(Set['a','b','c'], set)
end

assert("Set#to_a") do
  set = Set[1,2,3,2]
  ary = set.to_a

  assert_equal([1,2,3], ary.sort)
end

assert("Set#flatten") do
  # test1
  set1 = Set[
    1,
    Set[
      5,
      Set[7,
        Set[0]
      ],
      Set[6,2],
      1
    ],
    3,
    Set[3,4]
  ]

  set2 = set1.flatten
  set3 = Set.new(0..7)

  assert_false set1.equal?(set2) # assert_not_same
  assert_equal(set3, set2)


  # test2; multiple occurrences of a set in an set
  set1 = Set[1, 2]
  set2 = Set[set1, Set[set1, 4], 3]

  assert_nothing_raised {
    set3 = set2.flatten
  }

  assert_equal(Set.new(1..4), set3)


  # test3; recursion
  set2 = Set[]
  set1 = Set[1, set2]
  set2.add(set1)

  assert_raise(ArgumentError) {
    set1.flatten
  }

  # test4; miscellaneous
  empty = Set[]
  set = Set[Set[empty, "a"], Set[empty, "b"]]

  assert_nothing_raised {
    set.flatten
  }
end

assert("Set#flatten!") do
  # test1
  set1 = Set[
    1,
    Set[
      5,
      Set[7,
        Set[0]
      ],
      Set[6,2],
      1
    ],
    3,
    Set[3,4]
  ]

  set3 = Set.new(0..7)
  orig_set1 = set1
  set1.flatten!

  assert_true orig_set1.equal?(set1) # assert_same
  assert_equal(set3, set1)


  # test2; multiple occurrences of a set in an set
  set1 = Set[1, 2]
  set2 = Set[set1, Set[set1, 4], 3]

  assert_nothing_raised {
    set2.flatten!
  }

  assert_equal(Set.new(1..4), set2)


  # test3; recursion
  set2 = Set[]
  set1 = Set[1, set2]
  set2.add(set1)

  assert_raise(ArgumentError) {
    set1.flatten!
  }

  # test4; miscellaneous
  assert_nil(Set.new(0..31).flatten!)

  x = Set[Set[],Set[1,2]].flatten!
  y = Set[1,2]

  assert_equal(x, y)
end

assert("Set#include?") do
  set = Set[1,2,3]

  assert_true set.include?(1)
  assert_true set.include?(2)
  assert_true set.include?(3)
  assert_false set.include?(0)
  assert_false set.include?(nil)

  set = Set["1",nil,"2",nil,"0","1",false]
  assert_true set.include?(nil)
  assert_true set.include?(false)
  assert_true set.include?("1")
  assert_false set.include?(0)
  assert_false set.include?(true)
  assert_false set.include?(2)
end

assert("Set#superset?") do
  set = Set[1,2,3]

  assert_raise(ArgumentError) { set.superset?(nil) }
  assert_raise(ArgumentError) { set.superset?(2) }
  assert_raise(ArgumentError) { set.superset?([2]) }

  assert_true set.superset?(Set[])
  assert_true set.superset?(Set[1,2])
  assert_true set.superset?(Set[1,2,3])
  assert_false set.superset?(Set[1,2,3,4])
  assert_false set.superset?(Set[1,4])

  assert_true set >= Set[1,2]
  assert_true set >= Set[1,2,3]

  assert_true Set[].superset?(Set[])
end

assert("Set#proper_superset?") do
  set = Set[1,2,3]

  assert_raise(ArgumentError) { set.proper_superset?(nil) }
  assert_raise(ArgumentError) { set.proper_superset?(2) }
  assert_raise(ArgumentError) { set.proper_superset?([2]) }

  assert_true set.proper_superset?(Set[])
  assert_true set.proper_superset?(Set[1,2])
  assert_false set.proper_superset?(Set[1,2,3])
  assert_false set.proper_superset?(Set[1,2,3,4])
  assert_false set.proper_superset?(Set[1,4])

  assert_true set > Set[1,2]
  assert_false set > Set[1,2,3]

  assert_false Set[].proper_superset?(Set[])
end

assert("Set#subset?") do
  set = Set[1,2,3]

  assert_raise(ArgumentError) { set.subset?(nil) }
  assert_raise(ArgumentError) { set.subset?(2) }
  assert_raise(ArgumentError) { set.subset?([2]) }

  assert_true set.subset?(Set[1,2,3,4])
  assert_true set.subset?(Set[1,2,3])
  assert_false set.subset?(Set[1,2])
  assert_false set.subset?(Set[])

  assert_true set <= Set[1,2,3]
  assert_false set <= Set[1,2]

  assert_true Set[].subset?(Set[1])
  assert_true Set[].subset?(Set[])
end

assert("Set#proper_subset?") do
  set = Set[1,2,3]

  assert_raise(ArgumentError) { set.proper_subset?(nil) }
  assert_raise(ArgumentError) { set.proper_subset?(2) }
  assert_raise(ArgumentError) { set.proper_subset?([2]) }

  assert_true set.proper_subset?(Set[1,2,3,4])
  assert_false set.proper_subset?(Set[1,2,3])
  assert_false set.proper_subset?(Set[1,2])
  assert_false set.proper_subset?(Set[])

  assert_true set < Set[1,2,3,4]
  assert_false set < Set[1,2,3]

  assert_true Set[].proper_subset?(Set[1])
  assert_false Set[].proper_subset?(Set[])
end

assert("Set#intersect?") do
  set = Set[3,4,5]

  assert_raise(ArgumentError) { set.intersect?(3) }
  assert_raise(ArgumentError) { set.intersect?([2,4,6]) }

  assert_true set.intersect?(set)
  assert_true set.intersect?(Set[2,4])
  assert_true set.intersect?(Set[5,6,7])
  assert_true set.intersect?(Set[1,2,6,8,4])

  assert_false(set.intersect?(Set[]))
  assert_false(set.intersect?(Set[0,2]))
  assert_false(set.intersect?(Set[0,2,6]))
  assert_false(set.intersect?(Set[0,2,6,8,10]))

  # Make sure set hasn't changed
  assert_equal(Set[3,4,5], set)
end

assert("Set#disjoint?") do
  set = Set[3,4,5]

  assert_raise(ArgumentError) { set.disjoint?(3) }
  assert_raise(ArgumentError) { set.disjoint?([2,4,6]) }

  assert_true(set.disjoint?(Set[]))
  assert_true(set.disjoint?(Set[0,2]))
  assert_true(set.disjoint?(Set[0,2,6]))
  assert_true(set.disjoint?(Set[0,2,6,8,10]))

  assert_false set.disjoint?(set)
  assert_false set.disjoint?(Set[2,4])
  assert_false set.disjoint?(Set[5,6,7])
  assert_false set.disjoint?(Set[1,2,6,8,4])

  # Make sure set hasn't changed
  assert_equal(Set[3,4,5], set)
end

assert("Set#each") do
  ary = [1,3,5,7,10,20]
  set = Set.new(ary)

  ret = set.each { |o| }
  assert_true set.equal?(ret) # assert_same

  e = set.each
  assert_true e.instance_of?(Enumerator)

  assert_nothing_raised {
    set.each { |o|
      ary.delete(o) or raise "unexpected element: #{o}"
    }
    ary.empty? or raise "forgotten elements: #{ary.join(', ')}"
  }
end

assert("Set#add") do
  set = Set[1,2,3]

  ret = set.add(2)
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,3], set)

  ret = set.add(4)
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,3,4], set)
end

assert("Set#add?") do
  set = Set[1,2,3]

  ret = set.add?(2)
  assert_nil ret
  assert_equal(Set[1,2,3], set)

  ret = set.add?(4)
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,3,4], set)
end

assert("Set#delete") do
  set = Set[1,2,3]

  ret = set.delete(4)
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,3], set)

  ret = set.delete(2)
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,3], set)
end

assert("Set#delete?") do
  set = Set[1,2,3]

  ret = set.delete?(4)
  assert_nil ret
  assert_equal(Set[1,2,3], set)

  ret = set.delete?(1)
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[2,3], set)
end

assert("Set#delete_if") do
  set = Set.new(1..10)
  ret = set.delete_if { |i| i > 10 }
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set.new(1..10), set)

  set = Set.new(1..10)
  ret = set.delete_if { |i| i % 3 == 0 }
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,4,5,7,8,10], set)
end

assert("Set#keep_if") do
  set = Set.new(1..10)
  ret = set.keep_if { |i| i <= 10 }
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set.new(1..10), set)

  set = Set.new(1..10)
  ret = set.keep_if { |i| i % 3 != 0 }
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,4,5,7,8,10], set)
end

assert("Set#collect!") do
  set = Set[1,2,3,'a','b','c',-1..1,2..4]

  ret = set.collect! { |i|
    case i
    when Numeric
      i * 2
    when String
      i.upcase
    else
      nil
    end
  }

  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[2,4,6,"A","B","C",nil], set)
end

assert("Set#reject!") do
  set = Set.new(1..10)

  ret = set.reject! { |i| i > 10 }
  assert_nil(ret)
  assert_equal(Set.new(1..10), set)

  ret = set.reject! { |i| i % 3 == 0 }
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,4,5,7,8,10], set)
end

# this test is not in CRuby
assert("Set#select!") do
  set = Set.new(1..10)

  ret = set.select! { |i| i <= 10 }
  assert_nil(ret)
  assert_equal(Set.new(1..10), set)

  ret = set.select! { |i| i % 3 != 0 }
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,4,5,7,8,10], set)
end

assert("Set#merge") do
  set = Set[1,2,3]

  ret = set.merge([2,4,6])
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,2,3,4,6], set)
end

assert("Set#subtract") do
  set = Set[1,2,3]

  ret = set.subtract([2,4,6])
  assert_true set.equal?(ret) # assert_same
  assert_equal(Set[1,3], set)
end

assert("Set#+") do
  set = Set[1,2,3]

  ret = set + [2,4,6]
  assert_false set.equal?(ret) # assert_not_same
  assert_equal(Set[1,2,3,4,6], ret)
end

assert("Set#-") do
  set = Set[1,2,3]

  ret = set - [2,4,6]
  assert_false set.equal?(ret) # assert_not_same
  assert_equal(Set[1,3], ret)
end

assert("Set#&") do
  set = Set[1,2,3,4]

  ret = set & [2,4,6]
  assert_false set.equal?(ret) # assert_not_same
  assert_equal(Set[2,4], ret)
end

assert("Set#^") do
  set = Set[1,2,3,4]

  ret = set ^ [2,4,5,5]
  assert_false set.equal?(ret) # assert_not_same
  assert_equal(Set[1,3,5], ret)
end

assert("Set#==") do
  set1 = Set[2,3,1]
  set2 = Set[1,2,3]

  assert_equal(set1, set1)
  assert_equal(set1, set2)
  assert_not_equal(Set[1], [1])

  set1 = Class.new(Set)["a", "b"]
  set2 = Set["a", "b", set1]
  set1 = set1.add(set1.clone)

  assert_equal(set2, set2.clone)
  assert_equal(set1.clone, set1)
end

assert("Set#classify") do
  set = Set.new(1..10)
  ret = set.classify { |i| i % 3 }

  assert_equal(3, ret.size)
  assert_equal(Hash, ret.class)
  ret.each_value { |v| assert_equal(Set, v.class) }
  assert_equal(Set[3,6,9], ret[0])
  assert_equal(Set[1,4,7,10], ret[1])
  assert_equal(Set[2,5,8], ret[2])
end

assert("Set#divide") do
  # arity is 1
  set = Set.new(1..10)
  ret = set.divide { |i| i % 3 }

  assert_equal(3, ret.size)
  n = 0
  ret.each { |s| n += s.size }
  assert_equal(set.size, n)
  assert_equal(set, ret.flatten)

  assert_equal(Set, ret.class)
  assert_true(ret.include?(Set[3,6,9]))
  assert_true(ret.include?(Set[1,4,7,10]))
  assert_true(ret.include?(Set[2,5,8]))


  # arity is 2
  set = Set[7,10,5,11,1,3,4,9,0]
  assert_raise(NotImplementedError) {
    ret = set.divide { |a, b| (a - b).abs == 1 }
  }

  # assert_equal(4, ret.size)
  # n = 0
  # ret.each { |s| n += s.size }
  # assert_equal(set.size, n)
  # assert_equal(set, ret.flatten)

  # assert_equal(Set, ret.class)
end

# freeze is not implemented yet
#assert("freeze") do
#  orig = set = Set[1,2,3]
#  assert_equal false, set.frozen?
#  set << 4
#  assert_same orig, set.freeze
#  assert_equal true, set.frozen?
#  assert_raise(RuntimeError) {
#    set << 5
#  }
#  assert_equal 4, set.size
#end
#  assert("freeze_dup") do
#    set1 = Set[1,2,3]
#    set1.freeze
#    set2 = set1.dup
#
#    assert_not_predicate set2, :frozen?
#    assert_nothing_raised {
#      set2.add 4
#    }
#  end
#  assert("reeze_clone") do
#    set1 = Set[1,2,3]
#    set1.freeze
#    set2 = set1.clone
#
#    assert_predicate set2, :frozen?
#    assert_raise(RuntimeError) {
#      set2.add 5
#    }
#  end
#
assert("Set#inspect") do
  set = Set[1,2,3]
  assert_equal("#<Set: {1, 2, 3}>", set.inspect)
end
