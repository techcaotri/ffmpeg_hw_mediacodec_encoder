package com.pct.ffmpeg_hw_encoder

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.util.Log
import com.pct.ffmpeg_hw_encoder.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val extFile = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        if (!extFile?.exists()!!) {
            extFile.mkdir();
        }
        val resourceDir = extFile.absolutePath
        val imageResourceDir = "$resourceDir/images";
        PathTool.copyAssetsToDst(this, "images", imageResourceDir)
        // Example of a call to a native method
        Log.d("ffmpeg_hw_encoder", "resourceDir: $resourceDir")
        ConvertImagesToMp4(resourceDir)
    }

    /**
     * A native method that is implemented by the 'ffmpeg_hw_encoder' native library,
     * which is packaged with this application.
     */
    external fun ConvertImagesToMp4(prefix_path: String)

    companion object {
        // Used to load the 'ffmpeg_hw_encoder' library on application startup.
        init {
            System.loadLibrary("ffmpeg_hw_encoder")
        }
    }
}