##
# IO
#
# ISO 15.2.20

class IOError < StandardError; end
class EOFError < IOError; end

class IO
  def self.open(*args, &block)
    io = self.new(*args)

    return io unless block

    begin
      yield io
    ensure
      begin
        io.close unless io.closed?
      rescue StandardError
      end
    end
  end

  def self.popen(command, mode = 'r', **opts, &block)
    if !self.respond_to?(:_popen)
      raise NotImplementedError, "popen is not supported on this platform"
    end
    io = self._popen(command, mode, **opts)
    return io unless block

    begin
      yield io
    ensure
      begin
        io.close unless io.closed?
      rescue IOError
        # nothing
      end
    end
  end

  def self.pipe(&block)
    if !self.respond_to?(:_pipe)
      raise NotImplementedError, "pipe is not supported on this platform"
    end
    if block
      begin
        r, w = IO._pipe
        yield r, w
      ensure
        r.close unless r.closed?
        w.close unless w.closed?
      end
    else
      IO._pipe
    end
  end

  def self.read(path, length=nil, offset=0, mode: "r")
    str = ""
    fd = -1
    io = nil
    begin
      fd = IO.sysopen(path, mode)
      io = IO.open(fd, mode)
      io.seek(offset) if offset > 0
      str = io.read(length)
    ensure
      if io
        io.close
      elsif fd != -1
        IO._sysclose(fd)
      end
    end
    str
  end

  def hash
    # We must define IO#hash here because IO includes Enumerable and
    # Enumerable#hash will call IO#read() otherwise
    self.__id__
  end

  def <<(str)
    write(str)
    self
  end

  alias_method :eof, :eof?
  alias_method :tell, :pos

  def pos=(i)
    seek(i, SEEK_SET)
  end

  def rewind
    seek(0, SEEK_SET)
  end

  def ungetbyte(c)
    if c.is_a? String
      c = c.getbyte(0)
    else
      c &= 0xff
    end
    s = " "
    s.setbyte(0,c)
    ungetc s
  end

  # 15.2.20.5.3
  def each(&block)
    return to_enum unless block

    while line = self.gets
      block.call(line)
    end
    self
  end

  # 15.2.20.5.4
  def each_byte(&block)
    return to_enum(:each_byte) unless block

    while byte = self.getbyte
      block.call(byte)
    end
    self
  end

  # 15.2.20.5.5
  alias each_line each

  def each_char(&block)
    return to_enum(:each_char) unless block

    while char = self.getc
      block.call(char)
    end
    self
  end

  def puts(*args)
    i = 0
    len = args.size
    while i < len
      s = args[i]
      if s.kind_of?(Array)
        puts(*s)
      else
        s = s.to_s
        write s
        write "\n" if (s[-1] != "\n")
      end
      i += 1
    end
    write "\n" if len == 0
    nil
  end

  def print(*args)
    i = 0
    len = args.size
    while i < len
      write args[i].to_s
      i += 1
    end
  end

  def printf(*args)
    write sprintf(*args)
    nil
  end

  alias_method :to_i, :fileno
  alias_method :tty?, :isatty
end

STDIN  = IO.open(0, "r")
STDOUT = IO.open(1, "w")
STDERR = IO.open(2, "w")

$stdin  = STDIN
$stdout = STDOUT
$stderr = STDERR
