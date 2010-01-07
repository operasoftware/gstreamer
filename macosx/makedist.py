#!/usr/bin/env python
#
# read http://blogs.sun.com/dipol/entry/dynamic_libraries_rpath_and_mac for why we do this

import os
import os.path
import shutil
import sys
from subprocess import Popen, PIPE

def main(srcdir, dstdir, install_name_path):
    # copy dylib/so files to destination
    libs = {}
    for root, dirs, files in os.walk(srcdir):
        for file in files:
            srcpath = os.path.join(root, file)
            if (not os.path.islink(srcpath) and srcpath.split('.')[-1] in ['dylib', 'so']):
                dstpath = os.path.join(dstdir, srcpath[len(os.path.commonprefix([srcdir, srcpath])):])
                try:
                    os.makedirs(os.path.dirname(dstpath))
                except os.error:
                    # already exists?
                    pass
                shutil.copyfile(srcpath, dstpath)
                print 'Copying %s to %s' % (srcpath, dstpath)
                libs[dstpath] = {'old': Popen(['otool', '-X', '-D', dstpath],
                                                stdout=PIPE).communicate()[0].rstrip(),
                                   'new': install_name_path+file}

    # rewrite install names of libs
    for lib, names in libs.items():
        # change libraries own install name
        id_cmd = ['install_name_tool', '-id', names['new'], lib]
        print 'Executing ' + (' '.join(id_cmd))
        Popen(id_cmd).communicate()
        # change names of libraries it links to
        for link_names in libs.itervalues():
            change_cmd = ['install_name_tool', '-change',
                          link_names['old'], link_names['new'], lib]
            print 'Executing ' + (' '.join(change_cmd))
            Popen(change_cmd).communicate()

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print 'usage: %s [src-dir] [dst-dir] [install-name-path]' % sys.argv[0]
        exit(1)
    def slashify(s):
        if (s[-1] != '/'):
            s += '/'
        return s
    main(slashify(sys.argv[1]), slashify(sys.argv[2]), slashify(sys.argv[3]))
