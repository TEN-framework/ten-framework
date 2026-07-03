#script(lua)

prg = nil
loadstring = loadstring or load

function main(program)
  prg = program
  exec = ""
  repeat
    line = io.read()
    if line ~= nil then
      exec = exec..line.."\n"
      func = loadstring(exec)
      if func ~= nil then
        func()
        exec = ""
      end
    end
  until line == nil
end

#end.
