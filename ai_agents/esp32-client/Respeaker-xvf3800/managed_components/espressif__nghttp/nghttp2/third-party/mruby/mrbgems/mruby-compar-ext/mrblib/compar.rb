module Comparable
  ##
  #  call-seq:
  #    obj.clamp(min, max) ->  obj
  #    obj.clamp(range)    ->  obj
  #
  # In <code>(min, max)</code> form, returns _min_ if _obj_
  # <code><=></code> _min_ is less than zero, _max_ if _obj_
  # <code><=></code> _max_ is greater than zero, and _obj_
  # otherwise.
  #
  #    12.clamp(0, 100)         #=> 12
  #    523.clamp(0, 100)        #=> 100
  #    -3.123.clamp(0, 100)     #=> 0
  #
  #    'd'.clamp('a', 'f')      #=> 'd'
  #    'z'.clamp('a', 'f')      #=> 'f'
  #
  # In <code>(range)</code> form, returns _range.begin_ if _obj_
  # <code><=></code> _range.begin_ is less than zero, _range.end_
  # if _obj_ <code><=></code> _range.end_ is greater than zero, and
  # _obj_ otherwise.
  #
  #    12.clamp(0..100)         #=> 12
  #    523.clamp(0..100)        #=> 100
  #    -3.123.clamp(0..100)     #=> 0
  #
  #    'd'.clamp('a'..'f')      #=> 'd'
  #    'z'.clamp('a'..'f')      #=> 'f'
  #
  # If _range.begin_ is +nil+, it is considered smaller than _obj_,
  # and if _range.end_ is +nil+, it is considered greater than
  # _obj_.
  #
  #    -20.clamp(0..)           #=> 0
  #    523.clamp(..100)         #=> 100
  #
  # When _range.end_ is excluded and not +nil+, an exception is
  # raised.
  #
  #     100.clamp(0...100)       # ArgumentError
  #
  def clamp(min, max=nil)
    if max.nil?
      if min.kind_of?(Range)
        max = min.end
        if max.nil?
          max = self
        elsif min.exclude_end?
          raise ArgumentError, "cannot clamp with an exclusive range"
        end
        min = min.begin
        if min.nil?
          min = self
        end
      elsif min.nil? or min < self
        return self
      else
        return min
      end
    end
    if min.nil?
      if self < max
        return self
      else
        return max
      end
    end
    c = min <=> max
    if c.nil?
      raise ArgumentError, "comparison of #{min.class} with #{max.class} failed"
    elsif c > 0
      raise ArgumentError, "min argument must be smaller than max argument"
    end
    c = self <=> min
    if c.nil?
      raise ArgumentError, "comparison of #{self.class} with #{min.class} failed"
    elsif c == 0
      return self
    elsif c < 0
      return min
    end
    c = self <=> max
    if c.nil?
      raise ArgumentError, "comparison of #{self.class} with #{max.class} failed"
    elsif c > 0
      return max
    else
      return self
    end
  end
end
