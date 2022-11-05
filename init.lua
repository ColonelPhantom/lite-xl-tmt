-- mod-version:3 -- lite-xl 2.1
local core = require "core"
local keymap = require "core.keymap"
local command = require "core.command"
local common = require "core.common"
local config = require "core.config"
local style = require "core.style"
local View = require "core.view"

-- Import TMT; override CPATH to include DATADIR and USERDIR
local soname = PLATFORM == "Windows" and "?.dll" or "?.so"
local cpath = package.cpath
package.cpath = DATADIR .. '/plugins/tmt/' .. soname .. ';' .. package.cpath
package.cpath = USERDIR .. '/plugins/tmt/' .. soname .. ';' .. package.cpath
local libtmt = require "tmt"
package.cpath = cpath


config.plugins.tmt = {
    shell = os.getenv(PLATFORM == "Windows" and "COMSPEC" or "SHELL") or "/bin/sh",
    shell_args = {},
    split_direction = "down",
    resize_interval = 0.3 -- in seconds
}

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

local PASSTHROUGH_PATH = USERDIR .. "/plugins/tmt/pty"
local TERMINATION_MSG = "\r\n\n[Process ended with status %d]"

local shared_view = nil

local TmtView = View:extend()

function TmtView:new()
    TmtView.super.new(self)
    self.scrollable = false

    local args = { PASSTHROUGH_PATH, config.plugins.tmt.shell }
    for _, arg in ipairs(config.plugins.tmt.shell_args) do
        table.insert(args, arg)
    end
    self.proc = assert(process.start(args, {
        stdin = process.REDIRECT_PIPE,
        stdout = process.REDIRECT_PIPE
    }))

    self.tmt = libtmt.new(80,24)
    self.screen = {}

    self.title = "Tmt"
    self.visible = true
    self.scroll_region_start = 1
    self.scroll_region_end = self.rows

    self.term_target_size = { w = 80, h = 24 }

    self.alive = true

    core.add_thread(function()
        while self.alive do
            local output = self.proc:read_stdout()
            if not output then break end

            local events = self.tmt:write(output)
            core.redraw = events.screen
            self.bell = events.bell
            if events.answer then
                self:input_string(events.answer)
            end
            coroutine.yield(1 / config.fps)
        end

        self.alive = false
        self.tmt:write(string.format(TERMINATION_MSG, self.proc:returncode() or 0))
    end, self)
end

function TmtView:try_close(...)
    if self == shared_view then shared_view = nil end
    self.proc:kill()
    TmtView.super.try_close(self, ...)
end

function TmtView:get_name()
    return self.title
end

function TmtView:update(...)
    TmtView.super.update(self, ...)

    local sw, sh = self:get_screen_char_size()
    local tw, th = self.tmt:get_size()
    if sw ~= tw or sh ~= th then
        self.term_target_size.w, self.term_target_size.h = sw, sh
        if not self.resize_start then
            self.resize_start = system.get_time()
        end
    end

    if self.resize_start
        and (system.get_time() - self.resize_start > config.plugins.tmt.resize_interval) then
        self.resize_start = nil
        self.tmt:set_size(sw, sh)
        self.proc:write(string.format("\x1bXP%d;%dR\x1b\\", sh, sw))
    end

    -- update blink timer
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
        return command.perform "root:close"
    end
    self.proc:write(str)
end

function TmtView:get_screen_char_size()
    local font = style.code_font
    local x = self.size.x - style.padding.x
    local y = self.size.y - style.padding.y
    return math.max(1, math.floor(x / font:get_width("A"))),
        math.max(1, math.floor(y / font:get_height()))
end

local invisible = { ["\r"] = true, ["\n"] = true, ["\v"] = true, ["\t"] = true, ["\f"] = true, [" "] = true }
function TmtView:draw()
    self:draw_background(style.background)
    local font = style.code_font

    -- render screen
    local screen = self.tmt:get_screen(self.screen)
    local ox,oy = self:get_content_offset()
    local fw, fh = font:get_width("A"), font:get_height()

    ox, oy = ox + style.padding.x, oy + style.padding.y
    for i = 1, screen.width * screen.height do
        local cy = math.floor((i - 1) / screen.width)
        local cx = (i - 1) % screen.width

        local x, y = ox + cx * fw, oy + cy * fh
        local cell = screen[i]
        local char = cell.char
        if cell.bg ~= -1 then
            renderer.draw_rect(x, y, fw, fh, COLORS[cell.bg])
        end

        local fg = COLORS[cell.fg] or style.syntax.normal
        if not invisible[char] then
            renderer.draw_text(font, char, x, y, fg)
        end
    end

    -- render caret
    core.blink_timer = system.get_time()
    local T = config.blink_period
    if system.window_has_focus() then
        if config.disable_blink or core.active_view ~= self
            or (core.blink_timer - core.blink_start) % T < T / 2 then
            local cx, cy = self.tmt:get_cursor()
            local x, y = ox + (cx - 1) * fw, oy + (cy - 1) * fh
            renderer.draw_rect(x, y, style.caret_width, fh, style.caret)
        end
    end
end

-- override input handling

local macos = PLATFORM == "Mac OS X"
local modkeys_os = require("core.modkeys-" .. (macos and "macos" or "generic"))
local modkey_map = modkeys_os.map
local modkeys = modkeys_os.keys

local keymap_on_key_pressed = keymap.on_key_pressed
function keymap.on_key_pressed(k, ...)
    if not core.active_view:is(TmtView) then
        return keymap_on_key_pressed(k, ...)
    end

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
        local exceptions = {
            ["`"] = true,
        }
        if actions[k] then
            core.active_view:input_string(actions[k])
            return true
        elseif keymap.modkeys["ctrl"] and exceptions[k] then
            keymap_on_key_pressed(k, ...)
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

command.add(nil, {
    ["tmt:new"] = function()
        local node = core.root_view:get_active_node()
        node:add_view(TmtView())
    end,
    ["tmt:toggle"] = function()
        print("toggle")
        if not shared_view then
            -- create a terminal
            local node = core.root_view:get_active_node()
            if not shared_view then
                shared_view = TmtView()
            end
            node:split(config.plugins.tmt.split_direction, shared_view)
            core.set_active_view(shared_view)
        elseif core.active_view == shared_view then
            -- hide the terminal
            print("hiding")
            shared_view.visible = not shared_view.visible
            local node = core.root_view:get_active_node()
            node:remove_view(core.root_view.root_node, shared_view)
        else
            -- show and focus the terminal
            local node = core.root_view:get_active_node()
            if not shared_view.visible then
                shared_view.visible = true
                node:split(config.plugins.tmt.split_direction, shared_view)
            end
            core.set_active_view(shared_view)
        end
    end,
})

keymap.add({
    ["ctrl+t"] = "tmt:new",
    ["ctrl+`"] = "tmt:toggle"
})
