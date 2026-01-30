class Numeric
  ##
  #  call-seq:
  #    zero? -> true or false
  #
  #  Returns +true+ if +zero+ has a zero value, +false+ otherwise.
  #
  #  Of the Core and Standard Library classes,
  #  only Rational and Complex use this implementation.
  #
  def zero?
    self == 0
  end

  ##
  #  call-seq:
  #    nonzero?  ->  self or nil
  #
  #  Returns +self+ if +self+ is not a zero value, +nil+ otherwise;
  #  uses method <tt>zero?</tt> for the evaluation.
  #
  def nonzero?
    if self == 0
      nil
    else
      self
    end
  end

  ##
  #  call-seq:
  #    positive? -> true or false
  #
  #  Returns +true+ if +self+ is greater than 0, +false+ otherwise.
  #
  def positive?
    self > 0
  end

  ##
  #  call-seq:
  #    negative? -> true or false
  #
  #  Returns +true+ if +self+ is less than 0, +false+ otherwise.
  #
  def negative?
    self < 0
  end

  ##
  #  call-seq:
  #    int.allbits?(mask)  ->  true or false
  #
  #  Returns +true+ if all bits of <code>+int+ & +mask+</code> are 1.
  #
  def allbits?(mask)
    (self & mask) == mask
  end

  ##
  #  call-seq:
  #    int.anybits?(mask)  ->  true or false
  #
  #  Returns +true+ if any bits of <code>+int+ & +mask+</code> are 1.
  #
  def anybits?(mask)
    (self & mask) != 0
  end

  ##
  #  call-seq:
  #    int.nobits?(mask)  ->  true or false
  #
  #  Returns +true+ if no bits of <code>+int+ & +mask+</code> are 1.
  #
  def nobits?(mask)
    (self & mask) == 0
  end
end

class Integer
  #  call-seq:
  #    ceildiv(other) -> integer
  #
  #  Returns the result of division +self+ by +other+. The
  #  result is rounded up to the nearest integer.
  #
  #    3.ceildiv(3) # => 1
  #    4.ceildiv(3) # => 2
  #
  #    4.ceildiv(-3) # => -1
  #    -4.ceildiv(3) # => -1
  #    -4.ceildiv(-3) # => 2
  #
  #    3.ceildiv(1.2) # => 3
  def ceildiv(other)
    -div(-other)
  end
end
