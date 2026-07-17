#pragma once

namespace izan::keyd {

// Entry point for the trust-plane process. The UI spawns this binary
// again with:
//   --keyd-child --in=<tok> --out=<tok> --key=<tok> --vault=<path>
// where the tokens are inherited pipe handles: in/out carry the
// secretstream password channel, key delivers the one-shot 32-byte
// session key and is closed immediately after.
//
// Lifecycle: hardening first, then key pickup, then serve until
// Shutdown or until the channel breaks (parent died) — both paths
// wipe key material before exit.
int child_main(int argc, char** argv);

}
