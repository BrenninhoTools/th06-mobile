package com.brenninhotools.th06;

import org.libsdl.app.SDLActivity;

public class TH06Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "main"
        };
    }
}
