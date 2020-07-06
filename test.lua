local shared = require'shared'
local s = shared.object()

assert(#s, 'must be empty')

s['a']= 'hello'
shared.set(s, 'b', 'world')
assert(shared.get(s, 'a') == 'hello')
assert(s['b'] == 'world')
s[0]=1
s[1]=2
assert(s[0]==1)
assert(s[1]==2)
assert(shared.len(s)==2)
assert(shared.remove(s)==1)
assert(#s==1)
assert(shared.remove(s)==2)

assert(#s==0)
shared.insert(s, 1)
assert(s[0]==1)
assert(#s==1)
shared.insert(s, 2)
assert(s[1]==2)
assert(#s==2)
assert(s[2]==nil)
s[2]=3
assert(shared.get(s, 2)==3)
assert(s[2]==3)
assert(s[1]==2)
assert(#s==3)
assert(shared.remove(s, 2)==3)
assert(#s==2)
assert(shared.remove(s) == 1)
assert(#s==1)
assert(shared.remove(s)==2)
assert(shared.len(s)==0)
assert(tostring(s):match('^lua%.shared:'))
