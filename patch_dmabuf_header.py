#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import sys
import os

def patch_dmabuf_header(header_path):
    if not os.path.exists(header_path):
        print(f"Header file {header_path} does not exist")
        return False
    
    with open(header_path, 'r') as f:
        content = f.read()
    
    # Check if already patched
    if '#include "wayland-wayland-client-protocol.h"' in content:
        print("Header already patched")
        return True
    
    # Add the include at the beginning
    patched_content = '#include "wayland-wayland-client-protocol.h"\n' + content
    
    with open(header_path, 'w') as f:
        f.write(patched_content)
    
    print(f"Successfully patched {header_path}")
    return True

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: patch_dmabuf_header.py <header_file>")
        sys.exit(1)
    
    header_path = sys.argv[1]
    if patch_dmabuf_header(header_path):
        sys.exit(0)
    else:
        sys.exit(1)
