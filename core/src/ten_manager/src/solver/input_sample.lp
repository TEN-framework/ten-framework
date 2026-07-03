%
% Copyright © 2025 Agora
% This file is part of TEN Framework, an open source project.
% Licensed under the Apache License, Version 2.0, with certain conditions.
% Refer to the "LICENSE" file in the root directory for more information.
%

%-----------------------------------------------------------------------------
% Facts
%-----------------------------------------------------------------------------
root_declared("a").
root_declared("d").

version_declared("extension", "a", "1.2", 0).
version_declared("extension", "a", "1.1", 1).
version_declared("extension", "a", "1", 2).

version_declared("extension", "b", "2.2", 0).
version_declared("extension", "b", "2.1", 1).
version_declared("extension", "b", "2", 2).

version_declared("extension", "c", "3.2", 0).
version_declared("extension", "c", "3.1", 1).
version_declared("extension", "c", "3", 2).

version_declared("extension", "d", "4.2", 0).
version_declared("extension", "d", "4.1", 1).
version_declared("extension", "d", "4", 2).

version_declared("extension", "e", "101", 0).
version_declared("extension", "e", "100", 1).
version_declared("extension", "e", "99", 2).

depends_on_declared("extension", "a", "1.2", "extension", "b", "2").
depends_on_declared("extension", "a", "1.1", "extension", "b", "2.1").
depends_on_declared("extension", "a", "1.1", "extension", "c", "3").
depends_on_declared("extension", "a", "1", "extension", "b", "2.2").
depends_on_declared("extension", "b", "2", "extension", "d", "4").
depends_on_declared("extension", "c", "3", "extension", "d", "4").
depends_on_declared("extension", "d", "4.1", "extension", "e", "101").
