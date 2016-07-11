local gbc = cc.import("#gbc")
local HelloAction = cc.class("HelloAction", gbc.ActionBase)

function HelloAction:sayAction( args )
	local word = args.word or "world"
	return {result = "Hello, "..word}
end

return HelloAction