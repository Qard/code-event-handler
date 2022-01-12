const { env } = process

require('./').perfMap({
  path: env.PERF_MAP_PATH,
  accept: env.PERF_MAP_TYPES
})
