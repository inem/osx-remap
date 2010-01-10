#include <Carbon/Carbon.h>

struct flag_mapping {
  CGEventFlags bit;
  char *name;
};

#define ALT kCGEventFlagMaskAlternate
#define CMD kCGEventFlagMaskCommand
#define CTRL kCGEventFlagMaskControl
#define FN kCGEventFlagMaskSecondaryFn
#define NUM kCGEventFlagMaskNumericPad
#define SHIFT kCGEventFlagMaskShift
#define ASHIFT kCGEventFlagMaskAlphaShift

const struct flag_mapping event_flags[] = {
  { kCGEventFlagMaskAlphaShift, "alpha-shift" },
  { kCGEventFlagMaskShift, "shift" },
  { kCGEventFlagMaskControl, "ctrl" }, 
  { kCGEventFlagMaskAlternate, "alt" },
  { kCGEventFlagMaskCommand, "cmd" },
  /* Special key identifiers. */
  { kCGEventFlagMaskHelp, "help" },
  { kCGEventFlagMaskSecondaryFn, "fn" }, 
  /* Identifies key events from numeric keypad area on extended keyboards. */
  { kCGEventFlagMaskNumericPad, "numpad" },
//  /* Indicates if mouse/pen movement events are not being coalesced */
//  { kCGEventFlagMaskNonCoalesced, "noncoalesced" },
  {0,0}
};

void sig_handler(int sig) {
  CFRunLoopStop(CFRunLoopGetCurrent());
}

char *key_to_str(CGKeyCode keycode, CGEventFlags flags) {
  static char str[1024];
  char flags_str[1024] = {0};
  int i;

  for (i = 0; event_flags[i].name; i++) {
    if (flags & event_flags[i].bit) {
      strcat(flags_str, event_flags[i].name); strcat(flags_str, "-");
    }
  }

  sprintf(str, "%s%x", flags_str, keycode);

  return str;
}

void remap_keys(CGEventRef event, CGKeyCode keycode, CGEventFlags flags) {
  if (keycode == 0x30 && flags & CMD) {
    flags &= ~CMD;
    flags |= ALT;
  }

  CGEventFlags flagscopy = flags & (ALT|CMD|CTRL|FN|NUM|SHIFT|ASHIFT);

  // arrows
  if (keycode >= 0x7b && keycode <= 0x7e && flagscopy & (NUM|FN)) {
    // change spaces with cmd+ctrl+arrows instead of just cmd+arrows
    if (flagscopy == (CMD|CTRL|NUM|FN)) {
      flags &= ~CTRL;
    }
    // move win to diff space with shift+cmd+ctrl+arrows instead of just cmd+ctrl+arrows
    if (flagscopy == (CMD|CTRL|SHIFT|NUM|FN))
      flags &= ~SHIFT;
  }

  /* this has weird side effects, braindump:

     like hjitting ctrl-delete wchicj should now be fn-delete, didnt actually do the normal delete , it did a bakspace still
     like it turns out the fn key actually changes the keycode as well
     usually the delete key is 33
     but it returns 75 when u hit it with fn
     same with fn and the number pad
     and the thing that allows me tomove windows with ctrl-cmd-drag
     it doesnt work with my remap of fn to ctrl

  if (flags & CTRL) {
    flags &= ~CTRL;
    flags |= FN;
  } else if (flags & FN && ! (flags & NUM)) {
    flags &= ~FN;
    flags |= CTRL;
  }
  */

  printf("%s\n", key_to_str(keycode, flags));

  CGEventSetFlags(event, flags);
}

CGEventRef event_handler(CGEventTapProxy proxy, CGEventType ev_type, CGEventRef event, void *data) {
  CGEventSourceRef source;
  
  CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  CGEventFlags flags = CGEventGetFlags(event);

  if (!(source = CGEventCreateSourceFromEvent(event))) return event;

  printf("%s -> ", key_to_str(keycode, flags));
  remap_keys(event, keycode, flags);

  CFRelease(source);

  return event;
}

main() {
  CFMachPortRef port;
  CFRunLoopSourceRef source;

  if (!(port = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                (CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventFlagsChanged)),
                                 event_handler, NULL))) {
    printf("error creating tap\n"); exit(1);
  }

  if (!(source = CFMachPortCreateRunLoopSource(NULL, port, 0))) {
    printf("error creating source\n"); exit(1);
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);

  CFRelease(port);
  CFRelease(source);

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  CFRunLoopRun();
}
