#include <Carbon/Carbon.h>

struct flag_mapping {
  CGEventFlags bit;
  char *name;
};

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
  /* Indicates if mouse/pen movement events are not being coalesced */
  { kCGEventFlagMaskNonCoalesced, "noncoalesced" },
  {0,0}
};

CGEventRef event_handler(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *data) {
  CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  CGEventFlags flagbits = CGEventGetFlags(event);
  char flags[1024] = {0};
  int i;

  for (i = 0; event_flags[i].name; i++) {
    if (flagbits & event_flags[i].bit) {
      strcat(flags, event_flags[i].name); strcat(flags, " ");
    }
  }
  printf("%x flg: %s\n", keycode, flags);

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

  CFRunLoopRun();
}
