--[[

Copyright (c) 2015 gameboxcloud.com

Permission is hereby granted, free of chargse, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

]]

local Online = cc.import("#online")
local Session = cc.import("#session")

local gbc = cc.import("#gbc")
local UserAction = cc.class("UserAction", gbc.ActionBase)
local json = cc.import("#json")


local _opensession

function UserAction:auth( username, pass )
    local redis = self:getInstance():getRedis()
    local strUsers = redis:get("users")
    local users = json.decode(strUsers)
    local match = false
    for k,v in pairs(users) do
        if v.username == username and v.pass == pass then
            match = true
            break
        end
    end
    if match then
        return true
    else
        return false
    end
end

function UserAction:registerAction( args )
    local redis = self:getInstance():getRedis()
    local strUsers = redis:get("users")
    local users = {}
    if strUsers and strUsers ~="" then
        users = json.decode(strUsers)
        if users and #users > 1 then
            for k,v in pairs(users) do
                if v.username == args.username then
                    cc.throw("username in use")
                    return
                end
                if v.email == args.email then
                    cc.throw("email in use")
                    return
                end
            end
        end
    end
    local addUser = {}
    addUser.username = args.username
    addUser.pass = args.pass
    addUser.email = args.email
    addUser.gold = 5000
    addUser.photoIdx = #users
    if not users then 
        users = {}
        users[1] = addUser
    else
        table.insert(users, addUser)
    end
    strUsers = json.encode(users)
    redis:set("users", strUsers)
    return {success = "true"}
end

function UserAction:rsetusersAction( args )
    local redis = self:getInstance():getRedis()
    redis:set("users", nil)
    return true
end

function UserAction:getusersAction( args )
    local redis = self:getInstance():getRedis()
    local strUsers = redis:get("users")
    local users = {}
    if strUsers and strUsers ~="" then
        users = json.decode(strUsers)
    end
    return users
end

function UserAction:signinAction(args)
    --[[for k,v in pairs(args) do
        args = json.decode(k)
        break
    end]]
    local username = args.username
    local pass = args.pass

    if not username then
        cc.throw("not set argsument: \"username\"")
    end

    if not pass then
        cc.throw("not set argsument: \"pass\"")
    end

    if self:auth(username, pass) then

        -- start session
        local session = Session:new(self:getInstance():getRedis())
        session:start()
        session:set("username", username)
        session:set("count", 0)
        session:save()

        -- return result
        return {sid = session:getSid(), count = 0}
    else
        return {err = "not pass auth"}
    end
end

function UserAction:signoutAction(args)
    -- remove user from online list
    local session = _opensession(self:getInstance(), args)
    local online = Online:new(self:getInstance())
    online:remove(session:get("username"))
    -- delete session
    session:destroy()
    return {ok = "ok"}
end

function UserAction:countAction(args)
    -- update count value in session
    local session = _opensession(self:getInstance(), args)
    local count = session:get("count")
    count = count + 1
    session:set("count", count)
    session:save()

    return {count = count}
end

function UserAction:addjobAction(args)
    local sid = args.sid
    if not sid then
        cc.throw("not set argsument: \"sid\"")
    end

    local instance = self:getInstance()
    local redis = instance:getRedis()
    local session = Session:new(redis)
    if not session:start(sid) then
        cc.throw("session is expired, or invalid session id")
    end

    local delay = cc.checkint(args.delay)
    if delay <= 0 then
        delay = 1
    end
    local message = args.message
    if not message then
        cc.throw("not set argument: \"message\"")
    end

    -- send message to job
    local jobs = instance:getJobs()
    local job = {
        action = "/jobs/jobs.echo",
        delay  = delay,
        data   = {
            username = session:get("username"),
            message = message,
        }
    }
    local ok, err = jobs:add(job)
    if not ok then
        return {err = err}
    else
        return {ok = "ok"}
    end
end

-- private

_opensession = function(instance, args)
    local sid = args.sid
    if not sid then
        cc.throw("not set argsument: \"sid\"")
    end

    local session = Session:new(instance:getRedis())
    if not session:start(sid) then
        cc.throw("session is expired, or invalid session id")
    end

    return session
end

return UserAction
