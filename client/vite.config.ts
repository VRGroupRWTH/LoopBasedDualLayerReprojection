import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import path from "path";

const server_ip = "localhost";
const server_port = 9000;
const client_port = 3000;

export default defineConfig(
{
    root: "./website",
    plugins:
    [
        solidPlugin()
    ],
    define: 
    {
        server_ip: JSON.stringify(server_ip),
        server_port: JSON.stringify(server_port),
        client_port: JSON.stringify(client_port)
    },
    resolve:
    {
        alias:
        {
            "~bootstrap": path.resolve(__dirname, "node_modules/bootstrap"),
        }
    },
    server:
    {
        port: client_port,
        strictPort: true,
        proxy: 
        {
            "/server":
            {
                target: "http://" + server_ip + ":" + server_port,
                changeOrigin: true,
                rewrite: path => path.replace(/^\/server/, '')
            }
        },
        watch:
        {
            usePolling: true
        }
    }
});