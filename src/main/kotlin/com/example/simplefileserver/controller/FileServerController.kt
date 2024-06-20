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
//
//        request.characterEncoding = "UTF-8"
//        response.characterEncoding = "UTF-8"

        val path: String = rootPath + request.requestURI.replace("/file", "")
        val file = File(URLDecoder.decode(path, StandardCharsets.UTF_8))
        println(path)

        when {
            file.exists() -> {
                if (file.isDirectory) {
                    sendDirectoryList(file, response)
                } else {
                    serveFile(file, request, response)
                }
            }

            else -> sendNotFound(response)
        }
    }

    private fun sendNotFound(response: HttpServletResponse) {
        response.status = HttpServletResponse.SC_NOT_FOUND
        response.writer.println("HTTP/1.0 404 Not Found")
    }

    private fun serveFile(file: File, request: HttpServletRequest, response: HttpServletResponse) {
        val mimeType = getMimeType(file)
        response.contentType = mimeType
        response.setContentLengthLong(file.length())

        val contentDisposition = if (isInlineDisplay(mimeType)) "inline" else "attachment"
        response.setHeader("Content-Disposition", "$contentDisposition; filename=\"${file.name}\"")

        val rangeRequest = request.getHeader("Range") != null

        if (rangeRequest) {
            println("------------------serveFileInChunks-----------------")
            serveFileInChunks(file, request, response)
        } else {
            println("------------------serveFileNormally-----------------")
            serveFileNormally(file, response)
        }
    }

    /**
     * 普通下载方式
     */
    private fun serveFileNormally(file: File, response: HttpServletResponse) {
        val output = response.outputStream
        val input = file.inputStream()
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var bytesRead: Int
        while (input.read(buffer).also { bytesRead = it } != -1) {
            output.write(buffer, 0, bytesRead)
        }
        input.close()
    }

    /**
     * 分片下载方式
     *
     * --- Request Header ---
     * GET /example-file.txt HTTP/1.1
     * Host: www.example.com
     * Range: bytes=0-499
     *
     * --- Response Header ---
     * HTTP/1.1 206 Partial Content
     * Content-Range: bytes 0-499/1234 // start-end/size, 0-/1234 equals 0-499/1234
     * Content-Length: 500 // 注意此时content-length为：end - start + 1
     * Content-Type: text/plain
     */
    private fun serveFileInChunks(file: File, request: HttpServletRequest, response: HttpServletResponse) {
        val rangeHeader = request.getHeader("Range")
        val ranges = rangeHeader.removePrefix("bytes=").split("-") // 0-144 to ["0", "144"]; 0- to ["0", ""]
        val start = ranges[0].toLong()
        val end = if (ranges[1].isNotEmpty()) ranges[1].toLong() else file.length() - 1

        response.setStatus(HttpServletResponse.SC_PARTIAL_CONTENT)
        response.setContentLengthLong(end - start + 1)
        response.setHeader("Accept-Ranges", "bytes")
        response.setHeader("Content-Range", "bytes $start-$end/${file.length()}")

        val output = response.outputStream
        val input = file.inputStream()
        input.skip(start)
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var bytesToRead = end - start + 1

        while (bytesToRead > 0) {
            val bytesRead = input.read(buffer)
            if (bytesRead > bytesToRead) {
                output.write(buffer, 0, bytesToRead.toInt())// eg. 0-100/144, bytesToRead = 101, bytesRead = 145
                break
            }
            output.write(buffer, 0, bytesRead)
            bytesToRead -= bytesRead
        }
        input.close()
    }

    private fun getMimeType(file: File): String {
//        val contentType = URLConnection.guessContentTypeFromName(file.name)
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