module.exports = {
    apps:[
        {
            name: "web",
            script: "./server.js",
            instances: "max",
            exec_mode: "cluster",
            watch: true,
            env_development: {
                NODE_ENV: "development",
                PORT: 8000,
                // Add other environment variables here
            },
            env_production: {
                NODE_ENV: "production",
                PORT: 8000,
                // Add other environment variables here
            },
        }
    ]
};