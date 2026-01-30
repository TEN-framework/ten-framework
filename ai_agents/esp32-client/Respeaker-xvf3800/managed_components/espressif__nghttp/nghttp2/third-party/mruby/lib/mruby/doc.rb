autoload :Pathname, 'pathname'

module MRuby
  module Documentation
    def Documentation.update_opcode_md
      mrubydir = Pathname(MRUBY_ROOT)
      path_ops_h = mrubydir + "include/mruby/ops.h"
      path_opcode_md = mrubydir + "doc/internal/opcode.md"

      opspecs = {
        "Z" =>    { prefix: "", modified: "-" },
        "B" =>    { prefix: "\'" },
        "BB" =>   { prefix: "\"" },
        "BBB" =>  { prefix: "\"" },
        "BS" =>   { prefix: "\'" },
        "BSS" =>  { prefix: "\'" },
        "S" =>    { prefix: "" },
        "W" =>    { prefix: "" },
      }

      diff = ""

      spliter = <<~'SPLITER'
        | Instruction Name   | Operand type   | Semantics                                                  |
        |--------------------|----------------|------------------------------------------------------------|
      SPLITER

      diff = path_opcode_md.read.sub(/^#{Regexp.escape spliter}.*?(?=\z|^$\n)/m) do
        repl = spliter

        ops = path_ops_h.read
        ops.scan(/^\s*OPCODE\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)\s*(?:\/\*\s*(.*?)\s*\*\/\s*)?/).each do |ins, opr, cmt|
          if cmt
            cmt.sub!(/\s*#.*/, "")
            cmt.sub!(/\b(?=L_\w+\b)/, "OP_")
            cmt.gsub!(/\b(Irep|Pool|R|Syms)\[([^\[\]]+)\]/, "\\1(\\2)")
            cmt.gsub!(/[\\\|]/) { |m| "\\#{m}" } # Ruby-2.5 is not support "Numbered block parameter"
          end
          spec = opspecs[opr] or raise "unknown operand type: #{opr}"
          item = format(%(| %-18s | %-14s | %-58s |\n), "`OP_#{ins}`", "`#{spec[:modified] || opr}`", cmt && "`#{cmt}`")
          repl << item
        end

        repl
      end

      path_opcode_md.binwrite diff
    end
  end
end
