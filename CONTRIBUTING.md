# Contributing

The initial Community alpha accepts bug reports, hardware observations,
documentation improvements, and narrowly scoped patches.

Before submitting code:

1. open an issue describing the behavior and recovery impact;
2. keep firmware code freestanding C++20 with no exceptions, RTTI, or STL;
3. run `./Tools/test-host.sh`;
4. document new configuration fields and preserve backward compatibility; and
5. certify that you have the right to submit the contribution under GPL-3.0.

Do not commit proprietary firmware, signing keys, generated EDK II output, or
third-party artwork without documented redistribution permission.
