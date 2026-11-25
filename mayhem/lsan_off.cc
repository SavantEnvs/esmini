// Leaks are not the bug class this integration fuzzes for; keep ASan/UBSan, drop LSan.
extern "C" int __lsan_is_turned_off() { return 1; }
