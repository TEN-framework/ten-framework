assert 'Class#subclasses' do
  a = Class.new
  b = Class.new(a)
  c = Class.new(b)
  d = Class.new(a)

  a_sub = a.subclasses
  assert_equal(2, a_sub.size)
  assert_true(a_sub.include?(b))
  assert_true(a_sub.include?(d))
  assert_equal([c], b.subclasses)
  assert_equal([], c.subclasses)
end

assert 'Class#attached_object' do
  foo = Class.new
  foo_s = class <<foo
    foo_s = self
  end
  assert_equal(foo, foo_s.attached_object)
  fooi = foo.new
  fooi_s = class <<fooi
    fooi_s = self
  end
  assert_equal(fooi, fooi_s.attached_object)
  assert_raise(TypeError){foo.attached_object}
  assert_raise(TypeError){TrueClass.attached_object}
  assert_raise(TypeError){NilClass.attached_object}
end
