module Home where

import Protolude (
  Applicative (pure),
  Bool (..),
  Fractional ((/)),
  IO,
  Int,
  Maybe (Just, Nothing),
  Num,
  putText,
  (<&>),
 )

import Brillo.Interface.IO.Game as Gl (
  Event (..),
  Key (MouseButton),
  KeyState (Down),
  MouseButton (LeftButton),
 )
import Data.Text qualified as T

import TinyFileDialogs (openFileDialog)
import Types (AppState (..), ImageData (..), View (ImageView))
import Utils (isInRect, loadFileIntoState)


ticksPerSecond :: Int
ticksPerSecond = 10


data Message
  = ClickSelectFiles
  | OpenFileDialog


handleMsg :: Message -> AppState -> IO AppState
handleMsg msg appState =
  case msg of
    ClickSelectFiles -> do
      putText "ClickSelectFiles"
      pure appState
    OpenFileDialog -> do
      selectedFiles <-
        openFileDialog
          {- Title -} "Open File"
          {- Default path -} "/"
          {- File patterns -} ["*.jpeg", ".jpg", ".png"]
          {- Filter description -} "Image files"
          {- Allow multiple selects -} True

      case selectedFiles of
        Just files -> do
          let newState =
                appState
                  { currentView = ImageView
                  , images =
                      files <&> \filePath ->
                        ImageToLoad{filePath = T.unpack filePath}
                  }
          loadFileIntoState newState
        Nothing -> do
          putText "No file selected"
          pure appState


handleHomeEvent :: Event -> AppState -> IO AppState
handleHomeEvent event appState =
  case event of
    EventKey (MouseButton Gl.LeftButton) Gl.Down _ clickedPoint -> do
      let
        fileSelectBtnWidth :: (Num a) => a
        fileSelectBtnWidth = 120

        fileSelectBtnHeight :: (Num a) => a
        fileSelectBtnHeight = 40

        fileSelectBtnRect =
          ( -(fileSelectBtnWidth / 2)
          , -(fileSelectBtnHeight / 2)
          , fileSelectBtnWidth / 2
          , fileSelectBtnHeight / 2
          )
        fileSelectBtnWasClicked = clickedPoint `isInRect` fileSelectBtnRect

      if fileSelectBtnWasClicked
        then handleMsg OpenFileDialog appState
        else pure appState
    _ -> pure appState
