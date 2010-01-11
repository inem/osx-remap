#include <Carbon/Carbon.h>
#include <sys/time.h>

struct timeval last_event_1 = { 0, 0 }, last_event_2 = { 0, 0 };

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

void stop_loop() {
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
    // map cmd-arrow to alt-*
    if (flagscopy == (CMD|NUM|FN)) {
      flags &= ~CMD;
      flags |= ALT;
    }

    // change spaces with cmd+ctrl+arrows instead of just cmd+arrows
    if (flagscopy == (CMD|CTRL|NUM|FN))
      flags &= ~CTRL;

    // move win to diff space with shift+cmd+ctrl+arrows instead of just cmd+ctrl+arrows
    if (flagscopy == (CMD|CTRL|SHIFT|NUM|FN))
      flags &= ~SHIFT;
  }

  // cmd-backspace to alt-backspace
  if (keycode == 0x33 && flagscopy == CMD) {
    flags &= ~CMD;
    flags |= ALT;
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

void check_times() {
  long double t1 = last_event_1.tv_sec + ((long double)last_event_1.tv_usec/(long double)1000000.0);
  long double t2 = last_event_2.tv_sec + ((long double)last_event_2.tv_usec/(long double)1000000.0);

  printf("time diff %Lf\n", fabsl(t1-t2));
  // more than a 0.1 second delay between events, highly likely that one listener is dead
  if (fabsl(t1 - t2) > 0.1) stop_loop();
}

CGEventRef event_handler(CGEventTapProxy proxy, CGEventType ev_type, CGEventRef event, void *data) {
  gettimeofday(&last_event_1, NULL);

  CGEventSourceRef source;
  
  CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  CGEventFlags flags = CGEventGetFlags(event);

  if (!(source = CGEventCreateSourceFromEvent(event))) return event;

  printf("%s -> ", key_to_str(keycode, flags));
  remap_keys(event, keycode, flags);

  CFRelease(source);

  return event;
}

CGEventRef event_handler2(CGEventTapProxy proxy, CGEventType ev_type, CGEventRef event, void *data) {
  gettimeofday(&last_event_2, NULL);
  // TODO; get rid of this once it works
  printf(".");fflush(stdout); return event;
}

create_and_run_loop() {
  CFMachPortRef port, port2, port3;
  CFRunLoopSourceRef source, source2, source3;

  if (!(port = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                (CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventFlagsChanged)),
                                 event_handler, NULL))) {
    printf("error creating tap\n"); exit(1);
  }

  if (!(source = CFMachPortCreateRunLoopSource(NULL, port, 0))) {
    printf("error creating source\n"); exit(1);
  }

  if (!(port2 = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                                (CGEventMaskBit(kCGEventKeyDown)|CGEventMaskBit(kCGEventFlagsChanged)),
                                 event_handler2, NULL))) {
    printf("error creating tap\n"); exit(1);
  }

  if (!(source2 = CFMachPortCreateRunLoopSource(NULL, port2, 0))) {
    printf("error creating source\n"); exit(1);
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopDefaultMode);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), source2, kCFRunLoopDefaultMode);

  CFRelease(port);
  CFRelease(source);
  CFRelease(port2);
  CFRelease(source2);

  CFRunLoopRun();
}

void sig_handler(int sig) {
  if (sig == SIGINT || sig == SIGTERM) stop_loop();
  if (sig == SIGALRM) check_times();
}

main() {
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGALRM, sig_handler);

  struct itimerval it_old, it_val = { { 0, 500000 }, { 0, 500000 } };
  
  setitimer(ITIMER_REAL, &it_val, &it_old);

  create_and_run_loop();

  printf("exiting...\n");
}
