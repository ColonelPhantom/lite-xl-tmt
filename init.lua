-- mod-version:2 -- lite-xl 2.0
local core = require "core"
local keymap = require "core.keymap"
local command = require "core.command"
local common = require "core.common"
local config = require "core.config"
local style = require "core.style"
local View = require "core.view"

-- Import TMT; override CPATH to include DATADIR and USERDIR
package.cpath = DATADIR .. '/?.so;' .. package.cpath
package.cpath = USERDIR .. '/?.so;' .. package.cpath
local libtmt = require "plugins.tmt.tmt"


config.tmt = {
  shell = os.getenv(PLATFORM == "Windows" and "COMSPEC" or "SHELL") or "/bin/sh",
  shell_args = {},
  split_direction = "down"
}

local TmtView = View:extend()

local ESC = "\x1b"

local COLORS = {
    { common.color "#000000" },
    { common.color "#cc0000" },
    { common.color "#4e9a06" },
    { common.color "#c4a000" },
    { common.color "#3465a4" },
    { common.color "#75507b" },
    { common.color "#06989a" },
    { common.color "#d3d7cf" },
    { common.color "#555753" },
    { common.color "#ef2929" },
    { common.color "#8ae234" },
    { common.color "#fce94f" },
    { common.color "#729fcf" },
    { common.color "#ad7fa8" },
    { common.color "#34e2e2" },
    { common.color "#eeeeec" },

}

local function codepoint_to_utf8(c)
    assert((55296 > c or c > 57343) and c < 1114112, "Bad Unicode code point: "..c..".")
    if     c < 128 then
        return                                                          string.char(c)
    elseif c < 2048 then
        return                                     string.char(192 + c/64, 128 + c%64)
    elseif c < 55296 or 57343 < c and c < 65536 then
        return                    string.char(224 + c/4096, 128 + c/64%64, 128 + c%64)
    elseif c < 1114112 then
        return string.char(240 + c/262144, 128 + c/4096%64, 128 + c/64%64, 128 + c%64)
    end
end

local PASSTHROUGH_PATH = USERDIR .. "/plugins/tmt/pty"
local TERMINATION_MSG = "\r\n\n[Process ended with status %d]"

function TmtView:new()
    TmtView.super.new(self)
    self.scrollable = false

    local args = { PASSTHROUGH_PATH, config.tmt.shell }
    for _, arg in ipairs(config.tmt.shell_args) do
      table.insert(args, arg)
    end
    self.proc = assert(process.start(args, {
      stdin = process.REDIRECT_PIPE,
      stdout = process.REDIRECT_PIPE,
    }))

    self.tmt = libtmt.new(80,24)

    self.title = "Tmt"
    self.visible = true
    self.scroll_region_start = 1
    self.scroll_region_end = self.rows

    self.alive = true
end

function TmtView:try_close(...)
    self.proc:kill()
    TmtView.super.try_close(self, ...)
end

function TmtView:get_name()
    return self.title
end

function TmtView:update(...)
    TmtView.super.update(self, ...)

    -- handle output
    local output = ""
    local currently_alive = self.proc:running()
    if currently_alive then
        output = assert(self.proc:read_stdout())
    else
        if currently_alive ~= self.alive then
            self.alive = currently_alive
            output = string.format(TERMINATION_MSG, self.proc:returncode())
        end
    end

    if output:len() > 0 then
        core.redraw = true
        local events = self.tmt:write(output)
        for i,e in ipairs(events) do
            print(type(i), type(e), tostring(i), tostring(e))
            if e.type == "answer" then
                self:input_string(e.answer)
            elseif e.type == "bell" then
                -- bell not handled yet
                self.bell = true
            end
        end
    end

    -- update blink
    if self == core.active_view then
        local T, t0 = config.blink_period, core.blink_start
        local ta, tb = core.blink_timer, system.get_time()
        if ((tb - t0) % T < T / 2) ~= ((ta - t0) % T < T / 2) then
            core.redraw = true
        end
        core.blink_timer = tb
    end

end

function TmtView:on_text_input(text)
    self:input_string(text)
end

function TmtView:input_string(str)
    if not self.alive then
        command.perform "root:close"
        return
    end
    self.proc:write(str)
end

function TmtView:get_char_size()
    local font = style.code_font
    local x = self.size.x - 2*style.padding.x
    local y = self.size.y - 2*style.padding.y
    return math.floor(x / font:get_width(" ")), math.floor(y / font:get_height())
end

function TmtView:draw()
    self:draw_background(style.background)
    local font = style.code_font

    -- resize tmt
    local w,h = self:get_char_size()
    local tw, th = self.tmt:get_size()
    if w ~= tw or h ~= th then
        self.tmt.set_size(self.tmt, w,h)
    end

    -- render screen
    local screen = self.tmt:get_screen()
    local ox,oy = self:get_content_offset()
    local fw, fh = font:get_width(" "), font:get_height()
    for j = 1,screen.height do
        local y = oy + style.padding.y + (fh * (j-1))
        for i = 1,screen.width do
            local x = ox + style.padding.x + fw * (i-1)
            local char = screen.lines[j][i].char
            local letter = codepoint_to_utf8(char)
            local bg = screen.lines[j][i].bg
            if bg ~= -1 then
                renderer.draw_rect(x,y,fw,fh, COLORS[bg])
            end

            local fg = screen.lines[j][i].fg
            local fgc = COLORS[fg] or style.syntax.normal
            renderer.draw_text(style.code_font, letter, x, y, fgc)
        end
    end

    -- render caret
    if core.active_view == self then
        core.blink_timer = system.get_time()
        local T = config.blink_period
        if system.window_has_focus() then
            if config.disable_blink
            or (core.blink_timer - core.blink_start) % T < T / 2 then
                local cx, cy = self.tmt:get_cursor()
                renderer.draw_rect(ox + style.padding.x + fw*(cx-1) , oy+style.padding.y+fh*(cy-1), style.caret_width, fh, style.caret)
            end
        end
    end

end


-- override input handling
local function predicate()
    return getmetatable(core.active_view) == TmtView
end

local macos = PLATFORM == "Mac OS X"
local modkeys_os = require("core.modkeys-" .. (macos and "macos" or "generic"))
local modkey_map = modkeys_os.map
local modkeys = modkeys_os.keys

local keymap_on_key_pressed = keymap.on_key_pressed
function keymap.on_key_pressed(k)
    if not predicate() then
        return keymap_on_key_pressed(k)
    end
    local term = core.active_view
    local mk = modkey_map[k]
    if mk then
        -- keymap_on_key_pressed(k)
        keymap.modkeys[mk] = true
        -- work-around for windows where `altgr` is treated as `ctrl+alt`
        if mk == "altgr" then
            keymap.modkeys["ctrl"] = false
        end
    else
        local actions = {
            ["return"] = "\r",
            ["up"] = ESC .. "OA",
            ["down"] = ESC .. "OB",
            ["right"] = ESC .. "OC",
            ["left"] = ESC .. "OD",
            ["backspace"] = "\x7f",
            ["escape"] = "\x1b",
            ["tab"] = "\t",
            ["space"] = " ",
        }
        if actions[k] then
            core.active_view:input_string(actions[k])
            return true
        elseif keymap.modkeys["ctrl"] then
            local char = string.byte(k) - string.byte('a') + 1
            core.active_view:input_string(string.char(char))
            return true
        else
            return false
        end
    end
end

local keymap_on_key_released = keymap.on_key_released
function keymap.on_key_released(k)
  local mk = modkey_map[k]
  if mk then
    keymap_on_key_released(k)
    keymap.modkeys[mk] = false
  end
end


-- this is a shared session used by tmt:view
-- it is not touched by "tmt:open-here"
local shared_view = nil
local function shared_view_exists()
    return shared_view and core.root_view.root_node:get_node_for_view(shared_view)
end
command.add(nil, {
    ["tmt:new"] = function()
        local node = core.root_view:get_active_node()
        if not shared_view_exists() then
            shared_view = TmtView()
        end
        node:split(config.tmt.split_direction, shared_view)
        core.set_active_view(shared_view)
    end,
    ["tmt:toggle"] = function()
        if not shared_view_exists() then
            command.perform "tmt:new"
        else
            shared_view.visible = not shared_view.visible
            core.set_active_view(shared_view)
        end
    end,
    ["tmt:open-here"] = function()
        local node = core.root_view:get_active_node()
        node:add_view(TmtView())
    end
})

keymap.add({
    ["ctrl+t"] = "tmt:new",
    ["ctrl+`"] = "tmt:toggle"
})
