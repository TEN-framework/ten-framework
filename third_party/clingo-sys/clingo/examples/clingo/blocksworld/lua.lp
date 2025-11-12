#script(lua)

-- VARIABLES --

parts = {}

-- indicate solving approaches or grounding instructions, respectively
debugs = true -- false
debugg = false -- true

-- maximum time stamp for solving approaches
limit = math.huge

-- smallest and current time stamp
iinit = 0
time = 0

-- sequence number for blocks
block = 0

-- FUNCTIONS --

-- integrate a block appearing at time stamp "init"

function addblock(init)
  if limit < init then time = init end
  if limit < time then return end
  if init < iinit then
    for t = init+1, iinit do
      if debugg then print("Ground: table("..t..")") end
      parts[#parts+1] = {"table", {t}}
      for k = 1, block do
        if debugg then print("Ground: move("..k..","..t..")") end
        parts[#parts+1] = {"move", {k, t}}
      end
    end
    iinit = init
  end
  rollout(init)
  block = block+1
  if debugg then print("Ground: base_"..block) end
  parts[#parts+1] = {"base_"..block, {}}
  if debugg then print("Ground: init("..block..","..init..")") end
  parts[#parts+1] = {"init", {block, init}}
  if debugg then print("Ground: state("..block..","..init..")") end
  parts[#parts+1] = {"state", {block, init}}
  for t = iinit+1, time do
    if debugg then print("Ground: move("..block..","..t..")") end
    parts[#parts+1] = {"move", {block, t}}
    if init < t then
      if debugg then print("Ground: state("..block..","..t..")") end
      parts[#parts+1] = {"state", {block, t}}
    end
  end
end

-- ground encoding parts beyond current time stamp up to "next"

function rollout(next)
  if limit < next then
    time = next
    return
  end
  if time < next then
    time = time+1
    if debugg then print("Ground: table("..time..")") end
    parts[#parts+1] = {"table", {time}}
    for k = 1, block do
      if debugg then print("Ground: state("..k..","..time..")") end
      parts[#parts+1] = {"state", {k, time}}
      if debugg then print("Ground: move("..k..","..time..")") end
      parts[#parts+1] = {"move", {k, time}}
    end
    rollout(next)
  end
end

-- incrementally solve and ground for increasing time stamps until a plan exists

function incsolve()
  if limit < time then return end
  if debugs then
    print("=============")
    print("TIME "..time)
  end
  prg:ground(parts)
  parts = {}
  ret = prg:solve()
  if debugs then
    if     ret.satisfiable   then print "SATISFIABLE"
    elseif ret.unsatisfiable then print "UNSATISFIABLE"
    elseif ret.unknown       then print "UNKNOWN"
    end
    print("=============")
  end
  if ret.unsatisfiable then
    rollout(time+1)
    incsolve()
  end
end

-- AUXILIARY FUNCTIONS --

function setdebugg(debug)
  debugg = debug
end

function setdebugs(debug)
  debugs = debug
end

function setdebug(debug)
  debugg = debug
  debugs = debug
end

function setlimit(custom)
  limit = custom
end

function setiinit(custom)
  iinit = custom
  time = iinit
end

#end.
