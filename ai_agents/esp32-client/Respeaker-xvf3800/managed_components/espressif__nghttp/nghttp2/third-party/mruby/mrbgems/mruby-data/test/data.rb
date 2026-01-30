##
# Struct ISO Test

assert('Data') do
  assert_equal Class, Data.class
end

assert('Data.define') do
  c = Data.define(:m1, :m2)
  assert_equal Data, c.superclass
  assert_equal [:m1, :m2], c.members
end

assert('Data#==') do
  c = Data.define(:m1, :m2)
  cc1 = c.new(1,2)
  cc2 = c.new(1,2)
  assert_true cc1 == cc2
end

assert('Data#members') do
  c = Data.define(:m1, :m2)
  assert_equal [:m1, :m2], c.new(1,2).members
end

assert('wrong struct arg count') do
  c = Data.define(:m1)
  assert_raise ArgumentError do
    cc = c.new(1,2,3)
  end
end

assert('data dup') do
  c = Data.define(:m1, :m2, :m3, :m4, :m5)
  cc = c.new(1,2,3,4,5)
  assert_nothing_raised {
    assert_equal(cc, cc.dup)
  }
end

assert('Data inspect') do
  c = Data.define(:m1, :m2, :m3, :m4, :m5)
  cc = c.new(1,2,3,4,5)
  assert_equal "#<data m1=1, m2=2, m3=3, m4=4, m5=5>", cc.inspect
end

assert('Data#to_h') do
  s = Data.define(:white, :red, :green).new('ruuko', 'yuzuki', 'hitoe')
  assert_equal({:white => 'ruuko', :red => 'yuzuki', :green => 'hitoe'}) { s.to_h }
end

assert("Data.define does not allow array") do
  assert_raise(TypeError) do
    Data.define("Test", [:a])
  end
end

assert("Data.define generates subclass of Data") do
  begin
    original_struct = Data
    Data = String
    assert_equal original_struct, original_struct.define(:foo).superclass
  ensure
    Data = original_struct
  end
end

assert 'Data#freeze' do
  c = Data.define(:m)

  o = c.new(:test)
  assert_equal :test, o.m
  assert_nothing_raised {
    o.freeze
  }
end
