package com.pct.ffmpeg_hw_encoder

import android.content.Context
import android.content.res.AssetFileDescriptor
import android.net.Uri
import android.os.Environment
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream

class PathTool {
    companion object {
        /**
         * Copy the assets file to the specified directory on the phone
         * Context: application context
         * String srcPath: file name in assets directory
         * String sdPath: The path saved to the mobile phone directory
         */
        fun copyAssetToDst(context: Context, srcPath: String?, sdPath: String?) {
            try {
                val outFile = File(sdPath)
                if (outFile.exists()) {
                    outFile.delete()
                }
                outFile.createNewFile()
                val `is` = context.assets.open(srcPath!!)
                val fos = FileOutputStream(outFile)
                val buffer = ByteArray(1024)
                var byteCount: Int
                while (`is`.read(buffer).also { byteCount = it } != -1) {
                    fos.write(buffer, 0, byteCount)
                }
                DDlog.logd("finish")
                // Clear the buffer and force writing to the output file
                fos.flush()

                // Close the file handle; when an exception occurs, it will not be executed here.
                `is`.close()
                fos.close()
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        fun copyAssetsToDst(context: Context, srcAssetDirPath: String?, dstSDPath: String?) {
            val assetManager = context.assets
            try {
                val files = srcAssetDirPath?.let { assetManager.list(it) } ?: return

                val destDir = File(dstSDPath)
                if (!destDir.exists()) {
                    destDir.mkdirs()
                } else {
                    Log.d("PathTool", "images folder $dstSDPath already exists.")
                }

                for (filename in files) {
                    val assetFilePath =
                        if (srcAssetDirPath.isNullOrEmpty()) filename else "$srcAssetDirPath/$filename"
                    val destFilePath = "$dstSDPath/$filename"

                    try {
                        val inStream: InputStream = assetManager.open(assetFilePath)
                        val outFile = File(destFilePath)
                        val outStream: OutputStream = FileOutputStream(outFile)
                        copyFile(inStream, outStream)
                        inStream.close()
                        outStream.flush()
                        outStream.close()
                    } catch (e: IOException) {
                        // Handle exceptions here
                        Log.e("tag", "Failed to copy asset file: $filename", e)
                    }
                }
            } catch (e: IOException) {
                // Handle exceptions here
                Log.e("tag", "Failed to get asset file list.", e)
            }
        }

        private fun copyFile(inStream: InputStream, outStream: OutputStream) {
            val buffer = ByteArray(1024)
            var read: Int
            while (inStream.read(buffer).also { read = it } != -1) {
                outStream.write(buffer, 0, read)
            }
        }

        /** Android phone storage is divided into internal storage and external storage
         * 1. Internal storage; only the application itself and the super user can access it. Different device manufacturers may have different paths, but they are basically in the /data/xxx directory.
         * Obtained through the getxxx series functions of Context, such as getFilesDir(), etc.; the internal storage space is limited and will be deleted as the application is deleted.
         * 2. External storage; this storage may be removable storage media (such as SD card) or internal (non-removable) storage. Files saved to external storage can be accessed by all applications. There is no size limit, but they need to be
         * Manifest applies for read or write permission to read (READ_EXTERNAL_STORAGE) or write (WRITE_EXTERNAL_STORAGE). After 6.0, you need to apply for permission again when running.
         *
         */
        // Test results for Xiaomi 4
        fun testPath(context: Context) {
            // /data/user/0/com.media
            val dataFile = context.dataDir
            DDlog.logd("getDataDir() $dataFile")

            // ==== internal storage ===== //
            // /data/user/0/com.media/files
            val filesFile = context.filesDir
            DDlog.logd("getFilesDir $filesFile")
            // /data/user/0/com.media/cache
            val cacheFile = context.cacheDir
            DDlog.logd("getCacheDir $cacheFile")

            // /data/user/0/com.media/code_cache
            val shareFile = context.codeCacheDir
            DDlog.logd("getCodeCacheDir $shareFile")

            // ==== external storage ===== //

            // Get the external storage status; it can be read and written only when it is MEDIA MOUNTED.
            // Please refer to the document for specific status.
            // /storage/emulated/0
            val state = Environment.getExternalStorageState()
            DDlog.logd("getExternalStorageState $state")

            // Obtain the root path of the external storage; the path may be different for different mobile phone manufacturers.
            val extDirFile = Environment.getExternalStorageDirectory()
            DDlog.logd("getExternalStorageDirectory $extDirFile")

            // The default public directory of external storage; Android system provides ten public directories by default, which are stored in the external storage root path/xxx directory,
            // such as Environment.DIRECTORY_DOWNLOADS corresponding
            // External storage root path/Downloads, see related documents for others
            val pubFile =
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            DDlog.logd("getExternalStoragePublicDirectory $pubFile")

            // External storage private directory; like the application's internal storage directory, it will be deleted when the application is uninstalled.
            // It can be accessed by other applications. It is under the external storage directory/Android/data application package name/files/xxx directory name.
            val privFile = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)
            DDlog.logd("getExternalFilesDir $privFile")
            Environment.getRootDirectory()
        }
    }
}