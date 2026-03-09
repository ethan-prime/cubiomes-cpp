local ok, lspconfig = pcall(require, "lspconfig")
if not ok then
  return
end

local cfg = dofile(vim.fn.getcwd() .. "/.nvim/lsp/clangd.lua")
lspconfig.clangd.setup(cfg)
