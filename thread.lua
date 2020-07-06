local uv = require 'luv'
local shared = require'shared'
local S = shared.global()
local threads = {}

local function entry(I, J)
  local shared = require'shared'
  local S = shared.global()
  local s = shared.push(S)
  for i=I, J do
    s[i-I] = i+1
  end
end

threads[#threads+1] = uv.new_thread(entry, 0, 3)
threads[#threads+1] = uv.new_thread(entry, 4, 7)

for i=1, #threads do
  uv.thread_join(threads[i])
end
assert(#S==2)
local s = shared.pop(S)
assert(#S==1)
local cnt = 0
while s do
  for i=1, #s do
    cnt = cnt + 1
  end
  shared.release(s)
  s = shared.pop(S)
end
assert(#S==0)
assert(cnt==8)
uv.update_time()
print('DONE')

