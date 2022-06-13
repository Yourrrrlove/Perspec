{-# LANGUAGE QuasiQuotes #-}

module Main where

import Protolude as P

import System.Console.Docopt as Docopt
import System.Directory
  ( createDirectoryIfMissing
  , getXdgDirectory
  , listDirectory
  , makeAbsolute
  , renameFile
  , XdgDirectory(..)
  )
import System.FilePath ((</>))
import Data.Text as T (pack, unpack, isInfixOf)
import Data.Yaml (decodeFileEither, prettyPrintParseException)

import Lib (loadAndStart)
import Rename
import Types


patterns :: Docopt
patterns = [docoptFile|usage.txt|]


getArgOrExit :: Arguments -> Docopt.Option -> IO [Char]
getArgOrExit = getArgOrExitWith patterns


execWithArgs :: Config -> [[Char]] -> IO ()
execWithArgs config cliArgs = do
  args <- parseArgsOrExit patterns cliArgs

  when (args `isPresent` (command "fastfix")) $ do
    let files = args `getAllArgs` (argument "file")

    filesAbs <- sequence $ files <&> makeAbsolute

    let file = case filesAbs of
          [x] -> x
          x : _ -> x
          _ -> "This branch should not be reachable"

    loadAndStart (config { transformAppFlag = Hip }) file


  when (args `isPresent` (command "fix")) $ do
    let files = args `getAllArgs` (argument "file")

    filesAbs <- sequence $ files <&> makeAbsolute

    sequence_ $ filesAbs <&> loadAndStart config


  when (args `isPresent` (command "rename")) $ do
    directory <- args `getArgOrExit` (argument "directory")

    let
      startNumberMb = args `getArg` (longOption "start-with")
        <&> reads
        & (\case
              Just [(int, _)] -> Just int
              _ -> Nothing
          )

      renameMode =
        if args `isPresent` (longOption "even")
        then Even
        else
          if args `isPresent` (longOption "odd")
          then Odd
          else Sequential

      sortOrder =
        if args `isPresent` (longOption "descending")
        then Descending
        else Ascending

    files <- listDirectory directory

    let
      renamingBatches = getRenamingBatches
        startNumberMb
        renameMode
        sortOrder
        (files <&> pack)

    sequence_ $ renamingBatches
      <&> (\renamings -> do
            sequence_ $ renamings
              <&> (\(file, target) ->
                      renameFile
                        (directory </> unpack file)
                        (directory </> unpack target)
                  )
          )


main :: IO ()
main = do
  let appName = "Perspec"

  configDirectory <- getXdgDirectory XdgConfig appName
  createDirectoryIfMissing True configDirectory

  let configPath = configDirectory </> "config.yaml"

  configResult <- decodeFileEither configPath

  case configResult of
    Left error -> do
      if "file not found" `T.isInfixOf`
            (T.pack $ prettyPrintParseException error)
      then do
        writeFile configPath "licenseKey:\n"
        configResult2 <- decodeFileEither configPath

        case configResult2 of
          Left error2 -> die $ T.pack $ prettyPrintParseException error2
          Right config -> do
            getArgs >>= execWithArgs config
      else
        die $ T.pack $ prettyPrintParseException error

    Right config -> do
      getArgs >>= execWithArgs config


