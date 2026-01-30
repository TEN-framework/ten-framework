# This file tests fiber switching crossing C functions

unless RUBY_ENGINE == "mruby"
  class Fiber
    alias resume_by_c_func resume
    alias resume_by_c_method resume

    class << self
      alias yield_by_c_func yield

      def yield_by_c_method(*args)
        raise FiberError, "ycan't cross C function boundary"
      end
    end
  end

  def Proc.c_tunnel
    yield
  end
end

begin
  $fiber_test_activity = __FILE__

  assert('Call Fiber#resume nested with C') do
    assert_equal "ok1", Fiber.new { Fiber.new { "ok1" }.resume_by_c_func }.resume_by_c_func
    assert_equal "ok2", Fiber.new { Fiber.new { "ok2" }.resume_by_c_method }.resume_by_c_func
    assert_equal "ok3", Fiber.new { Fiber.new { "ok3" }.resume_by_c_func }.resume_by_c_method
    assert_equal "ok4", Fiber.new { Fiber.new { "ok4" }.resume_by_c_method }.resume_by_c_method
    assert_equal "ok5", Fiber.new { Proc.c_tunnel { Fiber.new { "ok5" }.resume_by_c_func } }.resume_by_c_func
    assert_equal "ok6", Fiber.new { Proc.c_tunnel { Fiber.new { "ok6" }.resume_by_c_method } }.resume_by_c_func
    assert_equal "ok7", Fiber.new { Proc.c_tunnel { Fiber.new { "ok7" }.resume_by_c_func } }.resume_by_c_method
    assert_equal "ok8", Fiber.new { Proc.c_tunnel { Fiber.new { "ok8" }.resume_by_c_method } }.resume_by_c_method
    assert_equal "ok9", Fiber.new { Proc.c_tunnel { Fiber.new { "ok9" }.resume } }.resume_by_c_func
    assert_equal "ok10", Fiber.new { Proc.c_tunnel { Fiber.new { "ok10" }.resume } }.resume_by_c_method
  end

  assert('Call Fiber#resume and Fiber.yield mixed with C.') do
    assert_equal 1, Fiber.new { Fiber.yield 1 }.resume_by_c_func
    assert_equal 2, Fiber.new { Fiber.yield 2 }.resume_by_c_method
    assert_equal 3, Fiber.new { Fiber.yield_by_c_func 3 }.resume
    assert_equal 4, Fiber.new { Fiber.yield_by_c_func 4 }.resume_by_c_func
    assert_equal 5, Fiber.new { Fiber.yield_by_c_func 5 }.resume_by_c_method
    assert_raise(FiberError) { Fiber.new { Fiber.yield_by_c_method "bad" }.resume }
    assert_raise(FiberError) { Fiber.new { Fiber.yield_by_c_method "bad" }.resume_by_c_func }
    assert_raise(FiberError) { Fiber.new { Fiber.yield_by_c_method "bad" }.resume_by_c_method }

    result = []
    f1 = Fiber.new { result << Fiber.new { Fiber.yield 1; "bad" }.resume_by_c_func; 2 }
    f2 = Fiber.new { result << f1.resume; 3 }
    result << f2.resume
    assert_equal [1, 2, 3], result

    f1 = Fiber.new {
      -> {
        Fiber.yield 1
        Fiber.yield_by_c_func 2
        f2 = Fiber.new {
          -> {
            Fiber.yield_by_c_func 3
            Fiber.yield 4
            Fiber.yield_by_c_func 5
            Fiber.yield 6
          }.call
          7
        }
        Fiber.yield f2.resume_by_c_func
        Fiber.yield f2.resume
        Fiber.yield f2.resume_by_c_method
        Fiber.yield f2.resume
        Fiber.yield f2.resume_by_c_func
        Fiber.yield 8
      }.call
      Fiber.yield 9
      10
    }
    result = []
    10.times { result << f1.resume }
    assert_equal [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], result
  end

  assert('Call Fiber#resume and Fiber.yield mixed with C and raising exceptions') do
    f = Fiber.new do
      raise ZeroDivisionError
    rescue
      Fiber.yield "rescue"
      "pass1"
    ensure
      Fiber.yield "ensure"
    end
    assert_equal "rescue", f.resume_by_c_method
    assert_equal "ensure", f.resume_by_c_method
    assert_equal "pass1", f.resume_by_c_method
    assert_raise(FiberError) { f.resume_by_c_method }

    f = Fiber.new do
      raise ZeroDivisionError
    rescue
      Fiber.yield "rescue"
      "pass2"
    ensure
      Fiber.yield "ensure"
    end
    assert_equal "rescue", f.resume_by_c_func
    assert_equal "ensure", f.resume_by_c_func
    assert_equal "pass2", f.resume_by_c_func
    assert_raise(FiberError) { f.resume_by_c_func }

    f2 = Fiber.new do
      -> do
        Fiber.yield 1
        raise "3"
      ensure
        Fiber.yield 2
      end.call
      "NOT REACH 1"
    end
    f1 = Fiber.new do
      Fiber.yield f2.resume_by_c_func
      begin
        Fiber.yield f2.resume
        Fiber.yield f2.resume_by_c_method
        Fiber.yield "NOT REACH 2"
      rescue => e
        Fiber.yield e.message
        Fiber.yield 4
      ensure
        Fiber.yield 5
      end
      Fiber.yield 6
      7
    end
    result = []
    7.times { result << f1.resume }
    assert_equal [1, 2, "3", 4, 5, 6, 7], result
  end

  assert('Call Fiber#transfer with C') do
    assert_equal "ok1", Fiber.new { Fiber.new { "ok1" }.resume_by_c_method }.transfer
    assert_equal "ok2", Fiber.new { Fiber.new { "ok2" }.resume_by_c_func }.transfer
    assert_raise(FiberError) { Proc.c_tunnel { Fiber.new { "BAD!" }.transfer } }

    b = Fiber.current
    a = Fiber.new {
      Proc.c_tunnel {
        Fiber.new {
          b.transfer
        }.resume
      }
    }
    assert_raise(FiberError) { a.transfer }
  end
ensure
  $fiber_test_activity = nil
end
