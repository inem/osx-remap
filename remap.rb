require 'gserver'

class Fixnum
  def has?(bit)
    self & bit == bit
  end
end

ASHIFT = 0x00010000
SHIFT = 0x00020000
CTRL = 0x00040000
ALT = 0x00080000
CMD = 0x00100000
NUM = 0x00200000
HELP = 0x00400000
FN = 0x00800000
NON = 0x00000100

KEY_NAMES = [
  [SHIFT, 'shift'],
  [CTRL, 'crtl'],
  [ALT, 'alt'],
  [CMD, 'cmd'],
  [FN, 'fn'],
  [NUM, 'num']
]

trap('INT') { puts "ctrl-C exit!"; exit }

class RemapServer < GServer
  def initialize(port)
    super(port, '127.0.0.1')
  end

  def serve(io)
    keycode, flags = io.gets.split(/\s+/).map {|s| s.strip.to_i }

    new_keycode, new_flags = remap_keys(keycode, flags)

    puts "#{key_to_str(keycode, flags)} -> #{key_to_str(new_keycode, new_flags)} .. [#{keycode}/#{flags}] -> [#{new_keycode}/#{new_flags}]"
    print dsl_remap = `/Users/steve/.rvm/ruby-1.9.1-p376/bin/ruby remap_dsl.rb #{keycode} #{flags}`
    if dsl_remap =~ /^(\d+) (\d+)$/
      io.puts "#$1 #$2"
    else
      io.puts "#{new_keycode} #{new_flags}"
    end
  end

  def remap_keys(keycode, oflags)
    key_bits = KEY_NAMES.map{|b,n| b }
    flags = oflags & key_bits.inject {|bits, bit| bits | bit }

    # cmd-tab -> alt-tab
    if keycode == 0x30 and flags.has? CMD
      oflags &= ~CMD
      oflags |= ALT
    end

    # arrows
    if (0x7b..0x7e).include? keycode and flags.has? NUM|FN
      # map ctrl-(shift)-arrow -> alt-(shift)-arrow
      if (flags&~SHIFT) == (CTRL|NUM|FN)
        oflags &= ~CTRL
        oflags |= ALT
      end

      # cmd-ctrl-arrows -> cmd-arrows (changign spaces)
      if flags == CMD|CTRL|NUM|FN
        oflags &= ~CTRL
      end

      # shift-cmd-ctrl-arrows -> cmd-ctrl-arrows (for moving windows between spaces)
      if flags == CMD|CTRL|SHIFT|NUM|FN
        oflags &= ~SHIFT
      end
    end

    # ctrl-backspace -> alt-backspace
    if keycode == 0x33 and flags == CTRL
      oflags &= ~CTRL
      oflags |= ALT
    end

    [keycode, oflags]
  end

  def key_to_str(keycode, flags)
    s = ''
    KEY_NAMES.map do |bit, name|
      s << name << '-' if ! (flags & bit).zero?
    end
    s << keycode.to_s
  end
end

s = RemapServer.new(9999)
s.start
s.join
