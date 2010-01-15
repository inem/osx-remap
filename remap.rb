require 'rubygems'
require 'eventmachine'
require 'pp'

MAPPINGS_FILE = File.expand_path('~/.remap_mappings.rb')

ASHIFT = 0x00010000
SHIFT = 0x00020000
CTRL = 0x00040000
ALT = 0x00080000
CMD = 0x00100000
NUM = 0x00200000
HELP = 0x00400000
FN = 0x00800000

NON = 0x00000100

SAVE_MASK = NON

LEFT_ALT = ALT|0x20
RIGHT_ALT = ALT|0x40

KEY_TO_NAME = {
  SHIFT => 'shift',
  CTRL => 'ctrl',
  ALT => 'alt',
  CMD => 'cmd',
  FN => 'fn',
  NUM => 'num'
}
NAME_TO_KEY = KEY_TO_NAME.invert

SYM_TO_KEYCODE = {
  :arrow => (0x7b..0x7e),
  :left => 0x7b,
  :right => 0x7c,
  :b => 11,
  :f => 3,
  :tab => 0x30,
  :backspace => 0x33
}

class Remapper
  class MappingsEval
    instance_methods.each {|m| undef_method m unless m =~ /^__|instance_eval/ }

    def only *a
      [:only, *a]
    end

    def except *a
      [:except, *a]
    end

    def method_missing m, *a, &b
      raise "bad config line at\n#{caller*"\n"}"  if a.size > 1
      $apps_scope = a.first  if a.size == 1
      $apps_scope = b.call  if b
      m.to_sym
    end

    def eval(&blk)
      $mappings = []
      self.instance_eval &blk
      ::Kernel.pp $mappings
      mappings, $mappings = $mappings, nil
      mappings.map {|m| Mapping.new(m) }
    end
  end

  class Mapping
    MASK = KEY_TO_NAME.keys.inject(0) {|mask, bit| mask | bit }
    def initialize(mapping)
      from_flags, to_flags, apps_scope = mapping
      from_keycode_sym = from_flags.pop
      to_keycode_sym = to_flags.pop

      @from_keycode = sym_to_keycode(from_keycode_sym)
      @to_keycode = sym_to_keycode(to_keycode_sym)

      @from_flags, @from_opt_flags = arr_to_flags(from_flags, from_keycode_sym)
      @to_flags, @to_opt_flags = arr_to_flags(to_flags, to_keycode_sym)

      if apps_scope
        @apps_scope_type = apps_scope.shift
        @apps_scope = apps_scope.map do |app|
          case app
          when Symbol
            /#{app}/i
          when Regexp
            /#{app.source}/i
          else
            app
          end
        end
      end
    end

    def remap_key(keycode, flags, process_name)
      saved = flags & SAVE_MASK
      flags &= MASK
      if matches?(keycode, flags, process_name)
        to_keycode = keycode_changes? ? @to_keycode : keycode
        to_flags = @to_flags|saved
        to_flags |= @to_opt_flags  if (flags & @from_opt_flags) != 0
        [to_keycode, to_flags]
      end
    end

    private

    def keycode_changes?
      @from_keycode != @to_keycode
    end

    def arr_to_flags(arr, keycode = nil)
      flags = opt_flags = 0
      arr.each  do |key|
        flags |= NAME_TO_KEY[key.to_s] || 0  if key.is_a? Symbol
        opt_flags |= NAME_TO_KEY[key.first.to_s] || 0  if key.is_a? Array
      end

      case keycode
      when :arrow, :left, :right
        flags |= NUM|FN
      end

      [flags, opt_flags]
    end

    def sym_to_keycode(sym)
      SYM_TO_KEYCODE[sym]
    end

    def matches?(keycode, flags, process_name)
      @from_keycode === keycode and @from_flags == flags&~@from_opt_flags and process_name_matches?(process_name)
    end

    def process_name_matches?(process_name)
      return true  if ! @apps_scope

      any = @apps_scope.any? {|a| a === process_name }
      @apps_scope_type == :only ? any : !any
    end
  end

  def remap(&blk)
    @mappings ||= []
    temp_defines do
      @mappings += MappingsEval.new.eval &blk
    end
  end

  def remap_key(keycode, flags, process_name)
    @mappings.each do |mapping|
      new_keycode, new_flags = mapping.remap_key(keycode, flags, process_name)
      return new_keycode, new_flags  if new_keycode
    end

    [keycode, flags]
  end

  private

  def temp_defines
    temp_define [Symbol, :-] => :symbol_minus,
                [Array, :-] => :array_minus,
                [Array, :>>] => :array_bit_shift do
      yield
    end
  end

  def temp_define(meths)
    old = {}
    meths.each do |(k, m), meth|
      b = send(meth)
      old[[k,m]] = "_#{rand(2**128).to_s(36)}"
      k.class_eval do
        alias_method old[[k,m]], m  if method_defined?(m)
        define_method m, &b
      end
    end
    yield
  ensure
    meths.each do |(k, m), b|
      k.class_eval do
        if old[[k,m]] and method_defined?(old[[k,m]])
          alias_method m, old[[k,m]]  
          remove_method old[[k,m]]
        end
      end
    end
  end

  def symbol_minus
    lambda {|s| [self, s] }
  end

  def array_minus
    lambda do |o|
      return self << o  if o.is_a? Symbol
      return self << "_#{o.first}_".to_sym  if is_a? Array
    end
  end

  def array_bit_shift
    lambda do |a|
      $mappings << [self, a, $apps_scope]
      $apps_scope = nil
    end
  end
end

def key_to_str(keycode, flags)
  s = ''
  KEY_TO_NAME.each do |bit, name|
    s << name << '-'  if flags & bit != 0
  end
  s << keycode.to_s
end

mapper = nil

load_mappings = lambda do
  mapper = Remapper.new
  eval(data=File.read(MAPPINGS_FILE), nil, MAPPINGS_FILE, 0)
  puts "loaded",data
end
load_mappings.call

EM.kqueue = true
EM.run do
  EM.start_server '127.0.0.1', 9999, Module.new {
    define_method :receive_data do |data|
      @buf ||= ''
      @buf << data
      if @buf =~ /^(\d+) (\d+) (.+?)$/
        keycode, flags, process_name = $1.to_i, $2.to_i, $3

        new_keycode, new_flags =  mapper.remap_key(keycode, flags, process_name)

        STDERR.puts "[#{process_name}] #{key_to_str(keycode, flags)} -> #{key_to_str(new_keycode, new_flags)}"
        STDERR.puts "ret: #{new_keycode.to_s 2} #{new_flags.to_s 2}"

        send_data "#{new_keycode} #{new_flags}\n"
      end
    end
  }

  register_file_watcher = lambda do
    EM.watch_file MAPPINGS_FILE, Module.new {
      define_method :file_modified do
        puts "reloading mappings"
        EM.add_timer(0.1) { load_mappings.call }
      end

      define_method :file_moved do
        file_modified
      end

      define_method :file_deleted do
        file_modified
      end

      define_method :unbind do
        EM.add_timer 0.1, &register_file_watcher
      end
    }
  end
  register_file_watcher.call

  EM.error_handler {|e| puts e, e.backtrace }
end
