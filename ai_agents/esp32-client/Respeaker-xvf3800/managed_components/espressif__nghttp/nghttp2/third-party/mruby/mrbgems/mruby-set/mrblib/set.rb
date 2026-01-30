class Set
  include Enumerable

  # internal method
  def __do_with_enum(enum, &block)
    if enum.respond_to?(:each)
      enum.each(&block)
    else
      raise ArgumentError, "value must be enumerable"
    end
  end

  # internal method to get internal hash
  def __get_hash
    @hash
  end

  def self.[](*ary)
    new(ary)
  end

  def initialize(enum = nil, &block)
    @hash ||= Hash.new

    enum.nil? and return

    if block_given?
      __do_with_enum(enum) { |o| add(block.call(o)) }
    else
      merge(enum)
    end
  end

  def initialize_copy(orig)
    super
    @hash = orig.__get_hash.dup
  end

  # def freeze
  #   @hash.freeze
  #   super
  # end

  def size
    @hash.size
  end
  alias length size

  def empty?
    @hash.empty?
  end

  def clear
    @hash.clear
    self
  end

  def replace(enum)
    clear
    merge(enum)
  end

  def to_a
    @hash.keys
  end

#  def to_set
#  end
#
  def flatten_merge(set, seen = Set.new)
    seen.add(set.object_id)
    set.each { |e|
      if e.is_a?(Set)
        if seen.include?(e_id = e.object_id)
          raise ArgumentError, "tried to flatten recursive Set"
        end

        flatten_merge(e, seen)
      else
        add(e)
      end
    }
    seen.delete(set.object_id)

    self
  end

  def flatten
    self.class.new.flatten_merge(self)
  end

  def flatten!
    if detect { |e| e.is_a?(Set) }
      replace(flatten())
    else
      nil
    end
  end

  def include?(o)
    @hash.include?(o)
  end
  alias member? include?
  alias === include?

  def superset?(set)
    raise ArgumentError, "value must be a set" unless set.is_a?(Set)
    return false if size < set.size
    set.all? { |o| include?(o) }
  end
  alias >= superset?

  def proper_superset?(set)
    raise ArgumentError, "value must be a set" unless set.is_a?(Set)
    return false if size <= set.size
    set.all? { |o| include?(o) }
  end
  alias > proper_superset?

  def subset?(set)
    raise ArgumentError, "value must be a set" unless set.is_a?(Set)
    return false if set.size < size
    all? { |o| set.include?(o) }
  end
  alias <= subset?

  def proper_subset?(set)
    raise ArgumentError, "value must be a set" unless set.is_a?(Set)
    return false if set.size <= size
    all? { |o| set.include?(o) }
  end
  alias < proper_subset?

  def intersect?(set)
    raise ArgumentError, "value must be a set" unless set.is_a?(Set)
    if size < set.size
      any? { |o| set.include?(o) }
    else
      set.any? { |o| include?(o) }
    end
  end

  def disjoint?(set)
    !intersect?(set)
  end

  def each(&block)
    return to_enum :each unless block_given?
    @hash.each_key(&block)
    self
  end

  def add(o)
    @hash[o] = true
    self
  end
  alias << add

  def add?(o)
    if include?(o)
      nil
    else
      add(o)
    end
  end

  def delete(o)
    @hash.delete(o)
    self
  end

  def delete?(o)
    if include?(o)
      delete(o)
    else
      nil
    end
  end

  def delete_if
    return to_enum :delete_if unless block_given?
    select { |o| yield o }.each { |o| @hash.delete(o) }
    self
  end

  def keep_if
    return to_enum :keep_if unless block_given?
    reject { |o| yield o }.each { |o| @hash.delete(o) }
    self
  end

  def collect!
    return to_enum :collect! unless block_given?
    set = self.class.new
    each { |o| set << yield(o) }
    replace(set)
  end
  alias map! collect!

  def reject!(&block)
    return to_enum :reject! unless block_given?
    n = size
    delete_if(&block)
    size == n ? nil : self
  end

  def select!(&block)
    return to_enum :select! unless block_given?
    n = size
    keep_if(&block)
    size == n ? nil : self
  end
  alias filter! select!

  def merge(enum)
    if enum.instance_of?(self.class)
      @hash.merge!(enum.__get_hash)
    else
      __do_with_enum(enum) { |o| add(o) }
    end

    self
  end

  def subtract(enum)
    __do_with_enum(enum) { |o| delete(o) }
    self
  end

  def |(enum)
    dup.merge(enum)
  end
  alias + |
  alias union |

  def -(enum)
    dup.subtract(enum)
  end
  alias difference -

  def &(enum)
    n = Set.new
    __do_with_enum(enum) { |o| n.add(o) if include?(o) }
    n
  end
  alias intersection &

  def ^(enum)
    (self | Set.new(enum)) - (self & Set.new(enum))
  end

  def ==(other)
    if self.equal?(other)
      true
    elsif other.instance_of?(self.class) && self.size == other.size
      @hash == other.__get_hash
    elsif other.is_a?(self.class) && self.size == other.size
      other.all? { |o| include?(o) }
    else
      false
    end
  end

  def <=>(set)
    return unless set.is_a?(Set)

    case size <=> set.size
    when -1 then -1 if proper_subset?(set)
    when +1 then +1 if proper_superset?(set)
    else 0 if self.==(set)
    end
  end

  def hash
    @hash.hash
  end

  def eql?(o)
    return false unless o.is_a?(Set)
    @hash.eql?(o.__get_hash)
  end

  def classify
    return to_enum :classify unless block_given?
    h = {}

    each { |i|
      x = yield(i)
      (h[x] ||= self.class.new).add(i)
    }

    h
  end

  def divide(&func)
    return to_enum :divide unless block_given?

    if func.arity == 2
      raise NotImplementedError, "Set#divide with 2 arity block is not implemented."
    end

    Set.new(classify(&func).values)
  end

  def join(separator = nil)
    to_a.join(separator)
  end

  def inspect
    return "#<#{self.class}: {}>" if empty?
    return "#<#{self.class}: {...}>" if self.__inspect_recursive?
    ary = map {|o| o.inspect }
    "#<#{self.class}: {#{ary.join(", ")}}>"
  end

  alias to_s inspect

  def reset
    if frozen?
      raise FrozenError, "can't modify frozen Set"
    else
      @hash.rehash
    end
  end
end
