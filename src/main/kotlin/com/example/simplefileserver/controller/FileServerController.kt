package com.example.simplefileserver.controller

import jakarta.servlet.http.HttpServletRequest
import jakarta.servlet.http.HttpServletResponse
import org.springframework.web.bind.annotation.RequestMapping
import org.springframework.web.bind.annotation.RestController
import java.io.File
import java.net.URLDecoder
import java.nio.charset.StandardCharsets

@RestController
@RequestMapping("/file")
class FileServerController {

    private val rootPath = "D:/" // 设置文件根目录

    @RequestMapping("/**")
    fun handleFileRequest(request: HttpServletRequest, response: HttpServletResponse) {
        val path: String = rootPath + request.requestURI.replace("/file", "")
        val file = File(URLDecoder.decode(path, StandardCharsets.UTF_8))
        println(path)

        if (file.exists()) {
            if (file.isDirectory) {
                sendDirectoryList(file, response)
            } else {
                downloadFile(file, response)
            }
        } else {
            response.writer.println("HTTP/1.0 404 Not Found")
        }

    }

    private fun downloadFile(file: File, response: HttpServletResponse) {
        val mimeType = getMimeType(file)
        response.contentType = mimeType
        response.setContentLengthLong(file.length())

        val contentDisposition = if (isInlineDisplay(mimeType)) "inline" else "attachment"
        response.setHeader("Content-Disposition", "$contentDisposition; filename=\"${file.name}\"")

        val output = response.outputStream
        val input = file.inputStream()
        // 将文件内容写入响应体
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var bytesRead: Int
        while (input.read(buffer).also { bytesRead = it } != -1) {
            output.write(buffer, 0, bytesRead)
        }

        input.close()
    }

    private fun getMimeType(file: File): String {
        val fileName = file.name.lowercase()
        return when {
            fileName.endsWith(".jpg") || fileName.endsWith(".jpeg") -> "image/jpeg"
            fileName.endsWith(".png") -> "image/png"
            fileName.endsWith(".gif") -> "image/gif"
            fileName.endsWith(".mp4") -> "video/mp4"
            fileName.endsWith(".pdf") -> "application/pdf"
            fileName.endsWith(".html") || fileName.endsWith(".htm") -> "text/html"
            fileName.endsWith(".txt") -> "text/plain"
            else -> "application/octet-stream"
        }
    }

    /**
     * 视频和图片文件直接在浏览器展示
     */
    private fun isInlineDisplay(mimeType: String): Boolean {
        return mimeType.startsWith("image/") || mimeType.startsWith("video/")
    }

    private fun sendDirectoryList(directory: File, response: HttpServletResponse) {
        response.contentType = "text/html; charset=UTF-8"

        val writer = response.writer

        writer.println("<html>")
        writer.println("<head>")
        writer.println("<title>" + directory.name + "</title>")
        writer.println("</head>")
        writer.println("<body>")

        writer.println("<h1>" + directory.name + "</h1>")

        writer.println("<ul>")
        val files = directory.listFiles()
        for (file in files!!) {
            if (file.isDirectory) {
                writer.println("<li><a href=\"" + file.name + "/" + "\">" + file.name + "/" + "</a></li>")
            } else {
                writer.println("<li><a href=\"" + file.name + "\">" + file.name + "</a></li>")
            }
        }
        writer.println("</ul>")

        writer.println("</body>")
        writer.println("</html>")

    }
}