package org.mariogolf64;

import org.libsdl.app.SDLActivity;

/**
 * SDL's activity, which runs the port's SDL_main. SDL is linked into
 * libmain.so with the port, so that is the only library to load.
 */
public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }
}
