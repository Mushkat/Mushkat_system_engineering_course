import fuse, time, errno, os
from stat import S_IFDIR, S_IFREG
import device

class CpuEmu(fuse.Operations):
    def __init__(self, cores):
        self.device = device.Device(cores)
        self.dirs = []
        self.files = {'/ctrl': self.device.ctrl}
        for i in range(cores):
            self.dirs.append('/unit{}'.format(i))
            self.files[self.dirs[-1] + '/lram'] = self.device.units[i].lram
            self.files[self.dirs[-1] + '/pram'] = self.device.units[i].pram

        creation_time = time.time()
        common_fields = dict(st_uid=os.getuid(), st_gid=os.getgid(), **{x:creation_time for x in ['st_ctime', 'st_mtime', 'st_atime']})
        self.dir_stat = dict(st_mode=(S_IFDIR | 0o755), st_nlink=1, **common_fields)
        self.file_stat = dict(st_mode=(S_IFREG | 0o644), st_nlink=1, st_size=0, **common_fields)

    def getattr(self, path, fh=None):
        if path == '/' or path in self.dirs:
            return self.dir_stat
        elif self.files.get(path) != None:
            return dict(self.file_stat, **{'st_size': len(self.files[path])})

        raise fuse.FuseOSError(errno.ENOENT)

    def readdir(self, path, fh):
        prefix = ['.', '..']
        if path == '/':
            return prefix + [d[1:] for d in self.dirs] + ['ctrl']
        elif path in self.dirs:
            return prefix + ['lram', 'pram']

        raise fuse.FuseOSError(errno.ENOENT)


    def read(self, path, size, offset, fh):
        if path == '/ctrl':
            val = self.files[path].wait()
            return bytes(''.join(map(lambda x: str(x) + '\n', val)).encode('utf-8'))

        return bytes(self.files[path][offset:offset + size])

    def write(self, path, data, offset, fh):
        if path == '/ctrl':
            self.files[path].append(int(data))
        else:
            self.files[path][offset:offset + len(data)] = data

        return len(data)

    def truncate(self, path, length, fh=None):
        if path != '/ctrl':
            self.files[path][:] = self.files[path][:length]
