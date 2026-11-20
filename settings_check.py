from storage import Store
store = Store("lesson-settings")
try:
    settings = store.settings("pvcpu")
    settings["winning_score"] = 5
    store.save_settings(settings)
    assert store.settings("pvcpu")["winning_score"] == 5
    print("Settings OK")
finally:
    store.close()
