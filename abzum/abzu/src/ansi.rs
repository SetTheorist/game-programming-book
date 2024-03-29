#![allow(non_snake_case)]

pub const FG_BLACK : &'static str = "\x1b[30m";
pub const FG_RED : &'static str = "\x1b[31m";
pub const FG_GREEN : &'static str = "\x1b[32m";
pub const FG_YELLOW : &'static str = "\x1b[33m";
pub const FG_BLUE : &'static str = "\x1b[34m";
pub const FG_MAGENTA : &'static str = "\x1b[35m";
pub const FG_CYAN : &'static str = "\x1b[36m";
pub const FG_WHITE : &'static str = "\x1b[37m";
pub const FG_DEFAULT : &'static str = "\x1b[39m";
#[inline]
pub fn FG_RGB(r:u8, g:u8, b:u8) -> String { format!("\x1b[38;2;{};{};{}m", r, g, b) }

pub const BG_BLACK : &'static str = "\x1b[40m";
pub const BG_RED : &'static str = "\x1b[41m";
pub const BG_GREEN : &'static str = "\x1b[42m";
pub const BG_YELLOW : &'static str = "\x1b[43m";
pub const BG_BLUE : &'static str = "\x1b[44m";
pub const BG_MAGENTA : &'static str = "\x1b[45m";
pub const BG_CYAN : &'static str = "\x1b[46m";
pub const BG_WHITE : &'static str = "\x1b[47m";
pub const BG_DEFAULT : &'static str = "\x1b[49m";
#[inline]
pub fn BG_RGB(r:u8, g:u8, b:u8) -> String { format!("\x1b[48;2;{};{};{}m", r, g, b) }

pub const RESET : &'static str = "\x1b[0m";
pub const NORMAL : &'static str = "\x1b[22m";
pub const BRIGHT : &'static str = "\x1b[1m";
pub const FAINT : &'static str = "\x1b[2m";
pub const UNDERLINE : &'static str = "\x1b[4m";

pub const HIDE_CURSOR : &'static str = "\x1b[?25l";
pub const SHOW_CURSOR : &'static str = "\x1b[?25h";

pub const CLEAR_LINE : &'static str = "\x1b[2K";
pub const CLEAR_SCREEN : &'static str = "\x1b[2J";
pub const CLEAR_TO_BOL : &'static str = "\x1b[1K";
pub const CLEAR_TO_EOL : &'static str = "\x1b[0K";
pub const CLEAR_TO_BOS : &'static str = "\x1b[1J";
pub const CLEAR_TO_EOS : &'static str = "\x1b[0J";

#[inline]
pub fn CURSOR_POSITION(x:usize,y:usize) -> String { format!("\x1b[{};{}H", x, y) }
#[inline]
pub fn CURSOR_UP(n:usize) -> String { format!("\x1b[{}A", n) }
#[inline]
pub fn CURSOR_DOWN(n:usize) -> String { format!("\x1b[{}B", n) }
#[inline]
pub fn CURSOR_FORWARD(n:usize) -> String { format!("\x1b[{}C", n) }
#[inline]
pub fn CURSOR_BACK(n:usize) -> String { format!("\x1b[{}D", n) }
#[inline]
pub fn CURSOR_NEXT_LINE(n:usize) -> String { format!("\x1b[{}E", n) }
#[inline]
pub fn CURSOR_PREV_LINE(n:usize) -> String { format!("\x1b[{}F", n) }

