package com.droiddeck.launcher.session

import android.net.LocalServerSocket
import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.os.ParcelFileDescriptor
import android.system.Os
import android.util.Log
import androidx.annotation.Keep
import com.droiddeck.launcher.core.SessionPart
import java.io.File
import java.io.FileDescriptor
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Android side of the Vortek forwarder (brief 003): listens on [sockDir]/V0 and hands each guest
 * client to libvortekrenderer (LGPL-2.1, tools/vortek). Replaces Winlator's XConnectorEpoll with one
 * small reader thread per client. Presentation is not wired (VORTEK_NO_XWINDOW, brief 003 Q1).
 */
class VortekServerComponent(val sockDir: File, private val options: Options = Options()) : SessionPart() {
    /** Field names and types are read by vk_context.c createVkContext() via JNI; keep them. */
    @Keep
    class Options {
        @JvmField var vkMaxVersion: Int = vkMakeVersion(1, 3, 128)
        @JvmField var maxDeviceMemory: Short = 0
        @JvmField var imageCacheSize: Short = IMAGE_CACHE_SIZE
        @JvmField var resourceMemoryType: Byte = 0
        @JvmField var exposedDeviceExtensions: Array<String>? = null
    }

    @Volatile private var running = false
    private var server: LocalServerSocket? = null
    private var listener: LocalSocket? = null
    private var acceptThread: Thread? = null
    private val clients = java.util.concurrent.ConcurrentHashMap<ParcelFileDescriptor, Long>()

    override fun start() {
        if (running) return
        if (!loaded) {
            Log.w(TAG, "libvortekrenderer missing; forwarder off")
            return
        }
        try {
            sockDir.mkdirs()
            val path = File(sockDir, VortekSelection.SOCKET_NAME)
            path.delete()
            initVulkanWrapper(app().applicationInfo.nativeLibraryDir, null)
            val socket = LocalSocket(LocalSocket.SOCKET_STREAM)
            socket.bind(LocalSocketAddress(path.path, LocalSocketAddress.Namespace.FILESYSTEM))
            listener = socket
            server = LocalServerSocket(socket.fileDescriptor)
            running = true
            acceptThread = Thread({ acceptLoop() }, "vortek-accept").apply { isDaemon = true; start() }
            Log.i(TAG, "listening on $path")
        } catch (e: Exception) {
            Log.w(TAG, "could not start", e)
            stop()
        }
    }

    override fun stop() {
        running = false
        runCatching { server?.close() }
        runCatching { listener?.close() }
        server = null
        listener = null
        acceptThread?.interrupt()
        acceptThread = null
        clients.keys.toList().forEach { pfd -> close(pfd) }
        File(sockDir, VortekSelection.SOCKET_NAME).delete()
    }

    private fun acceptLoop() {
        while (running) {
            val client = try { server?.accept() ?: break } catch (e: IOException) { break }
            val pfd = try {
                ParcelFileDescriptor.dup(client.fileDescriptor)
            } catch (e: IOException) {
                Log.w(TAG, "could not take client fd", e); null
            } finally {
                runCatching { client.close() }
            }
            if (pfd != null) Thread({ serve(pfd) }, "vortek-client-${pfd.fd}").apply { isDaemon = true; start() }
        }
    }

    private fun serve(pfd: ParcelFileDescriptor) {
        val fd = pfd.fd
        val jfd = pfd.fileDescriptor
        val header = ByteArray(8)
        try {
            while (running) {
                if (!readFully(jfd, header)) break
                val buf = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN)
                val code = buf.getInt(0)
                val length = buf.getInt(4)
                when {
                    code == REQUEST_CODE_CREATE_CONTEXT && !clients.containsKey(pfd) -> {
                        val ctx = createVkContext(fd, options)
                        if (ctx == 0L) break
                        clients[pfd] = ctx
                    }
                    code > Short.MAX_VALUE && (code shr 16) == REQUEST_CODE_SEND_EXTRA_DATA -> {
                        val ctx = clients[pfd] ?: break
                        if (!handleExtraDataRequest(ctx, code and 0xffff, length)) break
                    }
                    else -> { Log.w(TAG, "unexpected request $code"); break }
                }
            }
        } catch (e: Exception) {
            if (running) Log.w(TAG, "client $fd", e)
        } finally {
            close(pfd)
        }
    }

    @Synchronized
    private fun close(pfd: ParcelFileDescriptor) {
        clients.remove(pfd)?.let { destroyVkContext(it) }
        runCatching { pfd.close() }
    }

    private fun readFully(fd: FileDescriptor, into: ByteArray): Boolean {
        var off = 0
        while (off < into.size) {
            val n = Os.read(fd, into, off, into.size - off)
            if (n <= 0) return false
            off += n
        }
        return true
    }

    // The renderer resolves these with GetMethodID even though the stubbed swapchain never calls them.
    @Keep private fun getWindowWidth(windowId: Int): Int = 0
    @Keep private fun getWindowHeight(windowId: Int): Int = 0
    @Keep private fun getWindowHardwareBuffer(windowId: Int, bgra: Boolean): Long = 0
    @Keep private fun updateWindowContent(windowId: Int) {}

    private external fun createVkContext(clientFd: Int, options: Options): Long
    private external fun destroyVkContext(contextPtr: Long)
    private external fun initVulkanWrapper(nativeLibraryDir: String, libvulkanPath: String?)
    private external fun handleExtraDataRequest(contextPtr: Long, requestCode: Int, requestLength: Int): Boolean

    companion object {
        private const val TAG = "VortekServer"
        const val REQUEST_CODE_CREATE_CONTEXT = 1
        const val REQUEST_CODE_SEND_EXTRA_DATA = 2
        const val IMAGE_CACHE_SIZE: Short = 256

        fun vkMakeVersion(major: Int, minor: Int, patch: Int): Int = (major shl 22) or (minor shl 12) or patch

        private val loaded: Boolean = runCatching { System.loadLibrary("vortekrenderer") }.isSuccess
    }
}
