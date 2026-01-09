import axios from 'axios'

const genUUID = () => crypto.randomUUID()

export const apiGenAgoraData = async (config: { userId: number, channel: string }) => {
  const url = '/api/token/generate'
  const data = { request_id: genUUID(), uid: config.userId, channel_name: config.channel }
  const resp = await axios.post(url, data)
  const raw = resp.data || {}
  const code = raw.code ?? raw.status ?? (raw.success === true ? 0 : 1)
  const ok = code === 0 || code === '0' || code === 'success' || raw.success === true
  const msg = raw.msg ?? raw.message ?? raw.status ?? (ok ? 'ok' : 'error')
  return { ok, code, msg, data: raw.data }
}

export const apiStartService = async (config: { channel: string, userId: number, graphName?: string }) => {
  const url = '/api/agents/start'
  const data = {
    request_id: genUUID(),
    channel_name: config.channel,
    user_uid: config.userId,
    graph_name: config.graphName || 'diarization_demo',
  }
  const resp = await axios.post(url, data)
  const raw = resp.data || {}
  const code = raw.code ?? raw.status ?? (raw.success === true ? 0 : 1)
  const ok = code === 0 || code === '0' || code === 'success' || raw.success === true
  const msg = raw.msg ?? raw.message ?? raw.status ?? (ok ? 'ok' : 'error')
  return { ok, code, msg, data: raw.data }
}

export const apiStopService = async (channel: string) => {
  const url = '/api/agents/stop'
  const data = {
    request_id: crypto.randomUUID(),
    channel_name: channel,
  }
  const resp = await axios.post(url, data)
  return resp.data
}

export const apiPing = async (channel: string) => {
  const url = '/api/agents/ping'
  const data = {
    request_id: crypto.randomUUID(),
    channel_name: channel,
  }
  const resp = await axios.post(url, data)
  return resp.data
}
