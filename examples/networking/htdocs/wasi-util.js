import * as wasitype from "@bjorn3/browser_wasi_shim";

////////////////////////////////////////////////////////////
//
// event-related classes adopted from the on-going discussion
// towards poll_oneoff support in browser_wasi_sim project.
// Ref: https://github.com/bjorn3/browser_wasi_shim/issues/14#issuecomment-1450351935
//
////////////////////////////////////////////////////////////

export class EventType {
    /*:: variant: "clock" | "fd_read" | "fd_write"*/

    constructor(variant/*: "clock" | "fd_read" | "fd_write"*/) {
        this.variant = variant;
    }

    static from_u8(data/*: number*/)/*: EventType*/ {
        switch (data) {
        case wasitype.wasi.EVENTTYPE_CLOCK:
            return new EventType("clock");
        case wasitype.wasi.EVENTTYPE_FD_READ:
            return new EventType("fd_read");
        case wasitype.wasi.EVENTTYPE_FD_WRITE:
            return new EventType("fd_write");
        default:
            throw "Invalid event type " + String(data);
        }
    }

    to_u8()/*: number*/ {
        switch (this.variant) {
        case "clock":
            return wasitype.wasi.EVENTTYPE_CLOCK;
        case "fd_read":
            return wasitype.wasi.EVENTTYPE_FD_READ;
        case "fd_write":
            return wasitype.wasi.EVENTTYPE_FD_WRITE;
        default:
            throw "unreachable";
        }
    }
}

export class Event {
    /*:: userdata: UserData*/
    /*:: error: number*/
    /*:: type: EventType*/
    /*:: fd_readwrite: EventFdReadWrite | null*/

    write_bytes(view/*: DataView*/, ptr/*: number*/) {
        view.setBigUint64(ptr, this.userdata, true);
        view.setUint8(ptr + 8, this.error);
        view.setUint8(ptr + 9, 0);
        view.setUint8(ptr + 10, this.type.to_u8());
        // if (this.fd_readwrite) {
        //     this.fd_readwrite.write_bytes(view, ptr + 16);
        // }
    }

    static write_bytes_array(view/*: DataView*/, ptr/*: number*/, events/*: Array<Event>*/) {
        for (let i = 0; i < events.length; i++) {
            events[i].write_bytes(view, ptr + 32 * i);
        }
    }
}

export class SubscriptionClock {
    /*:: timeout: number*/

    static read_bytes(view/*: DataView*/, ptr/*: number*/)/*: SubscriptionFdReadWrite*/ {
        let self = new SubscriptionClock();
        self.timeout = Number(view.getBigUint64(ptr + 8, true));
        return self;
    }
}

export class SubscriptionFdReadWrite {
    /*:: fd: number*/

    static read_bytes(view/*: DataView*/, ptr/*: number*/)/*: SubscriptionFdReadWrite*/ {
        let self = new SubscriptionFdReadWrite();
        self.fd = view.getUint32(ptr, true);
        return self;
    }
}

export class SubscriptionU {
    /*:: tag: EventType */
    /*:: data: SubscriptionClock | SubscriptionFdReadWrite */

    static read_bytes(view/*: DataView*/, ptr/*: number*/)/*: SubscriptionU*/ {
        let self = new SubscriptionU();
        self.tag = EventType.from_u8(view.getUint8(ptr));
        switch (self.tag.variant) {
        case "clock":
            self.data = SubscriptionClock.read_bytes(view, ptr + 8);
            break;
        case "fd_read":
        case "fd_write":
            self.data = SubscriptionFdReadWrite.read_bytes(view, ptr + 8);
            break;
        default:
            throw "unreachable";
        }
        return self;
    }
}

export class Subscription {
    /*:: userdata: UserData */
    /*:: u: SubscriptionU */

    static read_bytes(view/*: DataView*/, ptr/*: number*/)/*: Subscription*/ {
        let subscription = new Subscription();
        subscription.userdata = view.getBigUint64(ptr, true);
        subscription.u = SubscriptionU.read_bytes(view, ptr + 8);
        return subscription;
    }

    static read_bytes_array(view/*: DataView*/, ptr/*: number*/, len/*: number*/)/*: Array<Subscription>*/ {
        let subscriptions = [];
        for (let i = 0; i < len; i++) {
            subscriptions.push(Subscription.read_bytes(view, ptr + 48 * i));
        }
        return subscriptions;
    }
}

export function wasiHackSocket(wasi, listenfd, connfd, sockAccept, sockSend, sockRecv) {
    // definition from wasi-libc https://github.com/WebAssembly/wasi-libc/blob/wasi-sdk-19/expected/wasm32-wasi/predefined-macros.txt
    const ERRNO_INVAL = 28;
    const ERRNO_AGAIN= 6;
    var connfdUsed = false;
    var connbuf = new Uint8Array(0);
    var _fd_close = wasi.wasiImport.fd_close;
    wasi.wasiImport.fd_close = (fd) => {
        if (fd == connfd) {
            connfdUsed = false;
            return 0;
        }
        return _fd_close.apply(wasi.wasiImport, [fd]);
    }
    var _fd_read = wasi.wasiImport.fd_read;
    wasi.wasiImport.fd_read = (fd, iovs_ptr, iovs_len, nread_ptr) => {
        if (fd == connfd) {
            return wasi.wasiImport.sock_recv(fd, iovs_ptr, iovs_len, 0, nread_ptr, 0);
        }
        return _fd_read.apply(wasi.wasiImport, [fd, iovs_ptr, iovs_len, nread_ptr]);
    }
    var _fd_write = wasi.wasiImport.fd_write;
    wasi.wasiImport.fd_write = (fd, iovs_ptr, iovs_len, nwritten_ptr) => {
        if (fd == connfd) {
            return wasi.wasiImport.sock_send(fd, iovs_ptr, iovs_len, 0, nwritten_ptr);
        }
        return _fd_write.apply(wasi.wasiImport, [fd, iovs_ptr, iovs_len, nwritten_ptr]);
    }
    var _fd_fdstat_get = wasi.wasiImport.fd_fdstat_get;
    wasi.wasiImport.fd_fdstat_get = (fd, fdstat_ptr) => {
        if ((fd == listenfd) || (fd == connfd) && connfdUsed){
            let buffer = new DataView(wasi.inst.exports.memory.buffer);
            // https://github.com/WebAssembly/WASI/blob/snapshot-01/phases/snapshot/docs.md#-fdstat-struct
            buffer.setUint8(fdstat_ptr, 6); // filetype = 6 (socket_stream)
            buffer.setUint8(fdstat_ptr + 1, 2); // fdflags = 2 (nonblock)
            return 0;
        }
        return _fd_fdstat_get.apply(wasi.wasiImport, [fd, fdstat_ptr]);
    }
    var _fd_prestat_get = wasi.wasiImport.fd_prestat_get;
    wasi.wasiImport.fd_prestat_get = (fd, prestat_ptr) => {
        if ((fd == listenfd) || (fd == connfd)){ // reserve socket-related fds
            let buffer = new DataView(wasi.inst.exports.memory.buffer);
            buffer.setUint8(prestat_ptr, 1);
            return 0;
        }
        return _fd_prestat_get.apply(wasi.wasiImport, [fd, prestat_ptr]);
    }
    wasi.wasiImport.sock_accept = (fd, flags, fd_ptr) => {
        if (fd != listenfd) {
            console.log("sock_accept: unknown fd " + fd);
            return ERRNO_INVAL;
        }
        if (connfdUsed) {
            console.log("sock_accept: multi-connection is unsupported");
            return ERRNO_INVAL;
        }
        if (!sockAccept()) {
            return ERRNO_AGAIN;
        }
        connfdUsed = true;
        var buffer = new DataView(wasi.inst.exports.memory.buffer);
        buffer.setUint32(fd_ptr, connfd, true);
        return 0;
    }
    wasi.wasiImport.sock_send = (fd, iovs_ptr, iovs_len, si_flags/*not defined*/, nwritten_ptr) => {
        if (fd != connfd) {
            console.log("sock_send: unknown fd " + fd);
            return ERRNO_INVAL;
        }
        var buffer = new DataView(wasi.inst.exports.memory.buffer);
        var buffer8 = new Uint8Array(wasi.inst.exports.memory.buffer);
        var iovecs = wasitype.wasi.Ciovec.read_bytes_array(buffer, iovs_ptr, iovs_len);
        var wtotal = 0
        for (var i = 0; i < iovecs.length; i++) {
            var iovec = iovecs[i];
            if (iovec.buf_len == 0) {
                continue;
            }
            var ret = sockSend(buffer8.subarray(iovec.buf, iovec.buf + iovec.buf_len));
            if (ret < 0) {
                return ERRNO_INVAL;
            }
            wtotal += iovec.buf_len;
        }
        buffer.setUint32(nwritten_ptr, wtotal, true);
        return 0;
    }
    wasi.wasiImport.sock_recv = (fd, iovs_ptr, iovs_len, ri_flags, nread_ptr, ro_flags_ptr) => {
        if (ri_flags != 0) {
            console.log("ri_flags are unsupported"); // TODO
        }
        if (fd != connfd) {
            console.log("sock_recv: unknown fd " + fd);
            return ERRNO_INVAL;
        }
        var buffer = new DataView(wasi.inst.exports.memory.buffer);
        var buffer8 = new Uint8Array(wasi.inst.exports.memory.buffer);
        var iovecs = wasitype.wasi.Iovec.read_bytes_array(buffer, iovs_ptr, iovs_len);
        var nread = 0;
        for (var i = 0; i < iovecs.length; i++) {
            var iovec = iovecs[i];
            if (iovec.buf_len == 0) {
                continue;
            }
            var retlen = sockRecv(buffer8, iovec.buf, iovec.buf_len);
            if ((retlen <= 0) && (i == 0)) {
                return ERRNO_AGAIN;
            }
            nread += retlen;
        }
        buffer.setUint32(nread_ptr, nread, true);
        // TODO: support ro_flags_ptr
        return 0;
    }
    wasi.wasiImport.sock_shutdown = (fd, sdflags) => {
        if (fd == connfd) {
            connfdUsed = false;
        }
        return 0;
    }
}
