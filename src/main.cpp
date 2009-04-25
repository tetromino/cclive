/*
 * cclive Copyright (C) 2009 Toni Gundogdu. This file is part of cclive.
 * 
 * cclive is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 * 
 * cclive is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <cstdlib>
#include <vector>

#include "except.h"
#include "app.h"

int
main (int argc, char *argv[]) {
    App app;

    int rc = EXIT_SUCCESS;
    try {
        app.main(argc, argv);
        app.run();
    }
    catch (const RuntimeException& x) {
        std::cerr << "error: " << x.getError() << std::endl;
        rc = EXIT_FAILURE;
    }
    return rc;
}