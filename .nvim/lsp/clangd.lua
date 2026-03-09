local root = vim.fn.getcwd()

return {
  cmd = {
    "clangd",
    "--background-index",
    "--clang-tidy",
    "--header-insertion=never",
    "--compile-commands-dir=" .. root,
  },
  root_dir = vim.fs.root(0, { ".git", "compile_commands.json" }),
}
