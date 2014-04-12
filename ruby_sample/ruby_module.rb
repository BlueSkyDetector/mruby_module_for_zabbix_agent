class Something
  def call()
    u = Userdata.new
    u.age += 1
    return "hello world " + u.name + " " + u.age.to_s
  end
end

def zbx_module_init()
  u = Userdata.new
  u.name = "test_user01"
  u.age = 30
  return 'init world'
end

def zbx_module_run()
  return Something.new.call()
end

def zbx_module_uninit()
  return 'uninit world'
end
