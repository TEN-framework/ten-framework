module Errno
  def Errno.const_defined?(name)
    __errno_defined?(name) or super
  end

  def Errno.const_missing(name)
    __errno_define(name) or super
  end

  # Module#constants is defined in mruby-metaprog
  # So, it may be raised NoMethodError
  def Errno.constants
    __errno_list(super)
  end
end
