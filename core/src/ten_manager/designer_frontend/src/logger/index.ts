//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import pino from "pino";

export const pinoLogger = pino({
  transport: {
    target: "pino-pretty",
    options: {
      colorize: true,
      translateTime: "SYS:standard",
      ignore: "hostname,pid",
    },
  },
  browser: {
    serialize: true,
    asObject: true,
    formatters: {
      level: (label) => ({ level: label }),
      log: ({ time, ...object }) => ({
        time: new Date(time as string).toLocaleString(),
        ...object,
      }),
    },
  },
});

export default pinoLogger;
