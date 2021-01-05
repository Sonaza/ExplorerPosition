# ExplorerPosition
A sneaky little program to monitor newly opening Explorer.exe windows in order to re-position them to the monitor where the mouse cursor is currently residing.

Program doesn't really have runtime configuration options.

However in code `positionUnderCursor` can be set to false in order for the program to not do any repositioning beyond moving the window to same relative coordinates on another monitor.

If the above is set to true additional margin values can be also tweaked to prevent the window from being positioned too close to the edges of monitor. Additional alternative margin can be used if cursor was positioned in task bar area when an explorer window opens (like in a case where new window is opened by middle clicking).
