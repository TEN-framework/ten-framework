assert("Binding#eval") do
  b = nil
  1.times { x, y, z = 1, 2, 3; [x,y,z]; b = binding }
  assert_equal([1, 2, 3], b.eval("[x, y, z]"))
  here = self
  assert_equal(here, b.eval("self"))
end

assert("Binding#local_variables") do
  block = Proc.new do |a|
    b = 1
    binding
  end
  bind = block.call(0)
  assert_equal [:a, :b, :bind, :block], bind.local_variables.sort
  bind.eval("x = 2")
  assert_equal [:a, :b, :bind, :block, :x], bind.local_variables.sort
end

assert("Binding#local_variable_set") do
  bind = binding
  1.times {
    assert_equal(9, bind.local_variable_set(:x, 9))
    assert_equal(9, bind.eval("x"))
    assert_equal([:bind, :x], bind.eval("local_variables.sort"))
  }
end

assert("Binding#local_variable_get") do
  bind = binding
  x = 1
  1.times {
    y = 2
    assert_equal(1, bind.local_variable_get(:x))
    x = 10
    assert_equal(10, bind.local_variable_get(:x))
    assert_raise(NameError) { bind.local_variable_get(:y) }
    bind.eval("z = 3")
    assert_equal(3, bind.local_variable_get(:z))
    bind.eval("y = 5")
    assert_equal(5, bind.local_variable_get(:y))
    assert_equal(2, y)
  }
end

assert "Binding#eval with Binding.new via UnboundMethod" do
  assert_raise(NoMethodError) { Class.instance_method(:new).bind_call(Binding) }
end

assert "Binding#eval with Binding.new via Method" do
  # The following test is OK if SIGSEGV does not occur
  cx = Class.new(Binding)
  cx.define_singleton_method(:allocate, &Object.method(:allocate))
  Class.instance_method(:new).bind_call(cx).eval("")

  assert_true true
end

assert "access local variables into procs" do
  bx = binding
  block = bx.eval("a = 1; proc { a }")
  bx.eval("a = 2")
  assert_equal 2, block.call
end

assert "Binding#eval on another target class" do
  obj = Object.new
  Module.new do
    self::BINDING = obj.instance_eval { binding }

    def self.eval(code)
      self::BINDING.eval code
    end

    self.eval "def self.m1; :m1; end"
    self.eval "def m2; :m2; end"
  end

  assert_equal :m1, obj.m1
  assert_equal :m2, obj.m2
end
