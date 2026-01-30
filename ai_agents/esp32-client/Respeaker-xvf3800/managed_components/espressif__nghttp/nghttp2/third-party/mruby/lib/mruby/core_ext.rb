autoload :Pathname, 'pathname'

class Object
  class << self
    def attr_block(*syms)
      syms.flatten.each do |sym|
        class_eval "def #{sym}(&block);block.call(@#{sym}) if block_given?;@#{sym};end"
      end
    end
  end
end

class String
  def relative_path_from(dir)
    Pathname.new(File.expand_path(self)).relative_path_from(Pathname.new(File.expand_path(dir))).to_s
  end

  def relative_path
    relative_path_from(Dir.pwd)
  end

  def remove_leading_parents
    Pathname.new(".#{Pathname.new("/#{self}").cleanpath}").cleanpath.to_s
  end

  def replace_prefix_by(dirmap)
    [self].replace_prefix_by(dirmap)[0]
  end
end

class Array
  # Replace the prefix of each string that is a file path that contains in its own array.
  #
  # dirmap is a hash whose elements are `{ "path/to/old-prefix" => "path/to/new-prefix", ... }`.
  # If it does not match any element of dirmap, the file path is not replaced.
  def replace_prefix_by(dirmap)
    dirmap = dirmap.map { |older, newer| [File.join(older, "/"), File.join(newer, "/")] }
    dirmap.sort!
    dirmap.reverse!
    self.flatten.map do |e|
      map = dirmap.find { |older, newer| e.start_with?(older) }
      e = e.sub(map[0], map[1]) if map
      e
    end
  end
end

def install_D(src, dst)
  _pp "INSTALL", src.relative_path, dst.relative_path
  rm_f dst
  mkdir_p File.dirname(dst)
  cp src, dst
end

def _pp(cmd, src, tgt=nil, indent: nil)
  return if Rake.application.options.silent

  width = 5
  template = indent ? "%#{width * indent}s %s %s" : "%-#{width}s %s %s"
  puts template % [cmd, src, tgt ? "-> #{tgt}" : nil]
end
