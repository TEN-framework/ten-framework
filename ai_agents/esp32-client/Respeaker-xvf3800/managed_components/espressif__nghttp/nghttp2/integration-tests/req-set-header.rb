class App
  def on_req(env)
    env.req.set_header "User-Agent", "mruby"
  end
end

App.new
