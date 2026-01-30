class App
  def on_resp(env)
    env.resp.set_header "Alpha", "bravo"
  end
end

App.new
