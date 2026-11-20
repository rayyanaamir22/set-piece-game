// Set Piece (native): see native/plan.md.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include "game/app.h"

int main(int argc, char** argv) {
    game::Options opts;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--seed") opts.seed = std::atoi(next());
        else if (a == "--shot") opts.shotPath = next();
        else if (a == "--frames") opts.frames = std::atoi(next());
        else if (a == "--demo-shoot") opts.demoShoot = true;
        else if (a == "--open") opts.openSheet = next();
        else if (a == "--prefs") opts.prefsOverride = next();
        else if (a == "--help") {
            std::printf("setpiece [--seed N] [--prefs FILE] [--frames N --shot out.png [--demo-shoot]]\n");
            return 0;
        }
    }
    try {
        game::App app(opts);
        return app.run();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "fatal: %s\n", ex.what());
        return 1;
    }
}
