<?php /* Copyright 2010-2021 Karl Wilcox, Mattias Basaglia

This file is part of the DrawShield.net heraldry image creation program

    DrawShield is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

     DrawShield is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with  DrawShield.  If not, see https://www.gnu.org/licenses/. */

include 'getcharge.inc';
include 'division.inc';
include 'effects.inc';
include "furs.inc";
include "svgtools.inc";
include "placecharge.inc";
include "linetypes.inc";
include "tinctures.inc";
include "ordinary.inc";
include "arrangements.inc";
include "quartering.inc";
include "shield.inc";
include "shapes.inc";
include "achievement.inc";
include "pathtools.inc";
include "treatment.inc";
include "getwords.inc";

function getLine($node)
{
    $retval = '';
    if ($node !== null) {
        $lineNo = $node->getAttribute(BlazonML::A_LINENUMBER);
        if ($lineNo != null) {
            $retval = " near line $lineNo";
        }
    }
    return $retval;
}


function makeOrdChgs($node, $layout)
{
    $retval = '';
    foreach ($node->childNodes as $child) {
        switch ($child->nodeName) {
            case 'ordinary':
                $retval .= makeOrdinary($child, $layout);
                break;
            case 'division':
                $retval .= makeDivision($child, $layout);
                break;
            case 'list':
                $retval .= makeList($child, $layout);
                break;
            case 'missing':
            case 'charge':
                $retval .= makeCharge($child, $layout);
                break;
        }
    }
    return $retval;
}

function setOptionsFromBlazon($node)
{
    global $options;

    foreach ($node->childNodes as $blazonOption) {
        switch ($blazonOption->nodeName) {
            case blazonML::E_COLOURSET:
                switch ($blazonOption->getAttribute('keyterm')) {
                    case 'web':
                        $options['useWebColours'] = true;
                        break;
                    case 'tartan':
                        $options['useTartanColours'] = true;
                        break;
                    case 'warhammer':
                        $options['useWarhammerColours'] = true;
                        break;
                    default:
                        // just ignore - should probably be an error message
                        break;
                }
                break;
            case blazonML::E_DEBUG:
                $options['debug'] = $blazonOption->getAttribute('keyterm');
                break;
            case blazonML::E_SHAPE:
                $options['shape'] = $blazonOption->getAttribute('keyterm');
                break;
            case blazonML::E_PALETTE:
                $options['palette'] = $blazonOption->getAttribute('keyterm');
                break;
            case blazonML::E_EFFECT:
                $options['effect'] = $blazonOption->getAttribute('keyterm');
                break;
            case blazonML::E_ASPECT:
                $ar = $blazonOption->getAttribute('keyterm');
                if ($ar != null) {
                    $options['aspectRatio'] = calculateAR($ar);
                }
                break;
        }
    }
    tidyOptions();
}

function drawShield($node, $layout) {
    global $options;

    $partsList = [];
    $body = makeshield($node, $layout);
    $layout = ShieldLayout::from_options($options);
    // options may have changed in makeShield
    $outline = getShape($layout);
    if (!array_key_exists('nomask', $options)) {
        $clipShield = add_def('mask', '<path fill="#FFFFFF" d="' . $outline . '" />');
        $body = '<g mask="url(#' . $clipShield . ')">' . $body . '</g>';
    }
    if ($options['effect'] == 'inked')
        $body .= '<path fill="none" stroke="#000000" stroke-width="4" d="' . $outline . '" />';
    foreach ($node->childNodes as $childNode) {
        if ($childNode->nodeName == blazonML::E_ACHIEVEMENT) {
            $partsList[] = getAchievementParts($childNode, $layout);
        }
    }
    return array ('body' => $body, 'parts' => $partsList);
}

function draw()
{
    global $dom;
    global $options;
    // global $version;
    global $messages;
    global $timings;

    $blazonNode = $dom->documentElement;
    // Shortcuts for special commands
    if ($blazonNode->hasChildNodes() && ($node = $blazonNode->firstChild) && $node->nodeName == blazonML::E_COMMAND) {
        switch ($node->getAttribute(blazonML::A_KEYTERM)) {
            case 'download':
                if ($node->hasChildNodes() && $node->firstChild->nodeName == blazonML::E_CHARGE) {
                    return getChargeDownload($node->firstChild);
                }
                break;
            default:
                break;
        }
    }

    // Get all the charge data (once for each charge type)
    get_chg_data();
    $mantling = '';
    $supports = '';
    $compartment = '';
    $motto = '';
    $ornament = '';
    $helmets = [];
    $crests = [];
    $fringe = '';
    $shieldAdjustments = [];
    $extraSpace = 0;
    $partsList = [];
    $vbHeight = null;
    $layout = ShieldLayout::from_options($options);
    if ($options['palette'] == 'hatching') $options['addHatching'] = $layout->size->height;

    $rdfTemplate = '<rdf:RDF>
      <cc:Work
         rdf:about="">
        <dc:format>image/svg+xml</dc:format>
        <dc:type
           rdf:resource="http://purl.org/dc/dcmitype/StillImage" />
        <dc:rights>
          <cc:Agent>
            <dc:title>This work is licensed under a &lt;a rel=&quot;license&quot; href=&quot;http://creativecommons.org/licenses/by-nc-sa/4.0/&quot;&gt;Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License&lt;/a&gt;.</dc:title>
          </cc:Agent>
        </dc:rights>
         <dc:creator>
           <cc:Agent>
              <dc:title>%CREDITS%</dc:title>
           </cc:Agent>
        </dc:creator>
      </cc:Work>
      </rdf:RDF>';

    $blazonNode = $dom->documentElement;
    $effect = $options['effect'];

    $svgHeight = (int)($options['size'] * 1.2);
    $svgWidth = $options['size'];

    $meta = '';
    $trailer = "</svg>\n"; // TODO this code is redundant..?
    $extraHeader = '';
    if ($options['palette'] == 'outline' || $options['palette'] == 'hatching') {
        $extraHeader .= '  <filter id="grayscale">
    <feColorMatrix type="saturate" values="0.10"/>
  </filter>';
    }

    $body = '';
    foreach ($blazonNode->childNodes as $node) {
        switch ($node->nodeName) {
            case blazonML::E_COMPLEX:
                $body .= makeAlongside($node, $layout);
                break;

            case blazonML::E_ROLL:
                // Rolls can take a long time to build, increase the timeout
                set_time_limit(60);
                $columns = $node->getAttribute(blazonML::A_COLS);
                $title = $node->getAttribute(blazonML::A_TITLE);
                $scale = 1 / $columns;
                $colCount = -1;
                $rowCount = 0;
                $gap = 100;
                $offset = ($gap /2) + $gap * $scale * -1;
                if (!is_null($title)) {
                    $builder = new WordBuilder();
                    $builder->setOption(WordBuilder::TEXT1, $title);
                    $builder->setOption(WordBuilder::ALIGN, WordBuilder::CENTRE);
                    $builder->setOption(WordBuilder::UNDERLINE, true);
                    // leave everything else as a default for now
                    $builder->compile();
                    $result = $builder->getSVG(true);
                    $capWidth = $result['width'];
                    $capHeight = $result['height'];
                    // work out how to scale & place it - it needs to go into a box 100 * 1000 at 0,100
                    // first, will it fit if we just scale the height?
                    $capScale = ($gap * 0.8) / $capHeight;
                    $newWidth = $capWidth * $capScale;
                    if ($newWidth > 1000) { // fit width instead
                        $capScale = 940 / $capWidth;
                        $newWidth = 940;
                    }
                    $xShift = (1000 - $newWidth) / 2;
                    $yShift = ($gap * 0.1);
                    $body .= "<g transform=\"translate($xShift,$yShift) scale($capScale)\">{$result['svg']}</g>";
                    $offset += 100;
                }
                $background = null;
                foreach ($node->childNodes as $childNode) {
                    if ($childNode->nodeName == blazonML::E_TINCTURE) {
                        $background = $childNode;
                        continue;
                    }
                    $colCount += 1;
                    if ($colCount >= $columns) {
                        $colCount = 0;
                        $rowCount += 1;
                        $offset += 100 * $scale;
                    }
                    if ($childNode->nodeName != blazonML::E_MISSING) {
                        $captionSVG = '';
                        $captionAboveShift = 0;
                        // first create a caption if it is given
                        if ($caption = $childNode->getAttribute(blazonML::A_CAPTION)) {
                            $captionAboveShift = 0;
                            $position = $childNode->getAttribute(blazonML::A_CAPTIONPOS);
                            $builder = new WordBuilder();
                            $builder->setOption(WordBuilder::TEXT1, $caption);
                            $builder->setOption(WordBuilder::ALIGN, WordBuilder::CENTRE);
                            // leave everything else as a default for now
                            $builder->compile();
                            $result = $builder->getSVG(true);
                            $capWidth = $result['width'];
                            $capHeight = $result['height'];
                            // work out how to scale & place it - it needs to go into a box 99 * 1000 at 0,1200
                            // first, will it fit if we just scale the height?
                            $capScale = ($gap * 0.8) / $capHeight;
                            $newWidth = $capWidth * $capScale;
                            if ($newWidth > 1000) { // fit width instead
                                $capScale = 940 / $capWidth;
                                $newWidth = 940;
                            }
                            // figure out where it will go
                            $xPos = (1000 - $newWidth) / 2;
                            if ($position == 'below') {
                                $yPos = 1200 + ($gap * 0.1);
                            } else { // i.e. "above" - use swith if others
                                $yPos = ($gap * 0.1);
                                $captionAboveShift = 100;
                            }
                            $captionSVG = "<g transform=\"translate($xPos,$yPos) scale($capScale)\">{$result['svg']}</g>";
                        }
                        $xShift = $colCount * (1000 * $scale);
                        $yShift = $offset + $rowCount * (1200 * $scale);
                        $entry = drawShield($childNode, $layout);
                        $rollEntry = $entry['body'];
                        if (count($partsList)) {
                            foreach ($partsList as $parts) {
                                $rollEntry .= addParts($rollEntry, $parts, $layout);
                            }
                        }
                        if ($captionAboveShift) {
                            $rollEntry = "<g transform=\"translate(0,$captionAboveShift)\">$rollEntry</g>";
                        }
                        $rollEntry = "<g transform=\"translate($xShift,$yShift) scale($scale)\">$captionSVG$rollEntry</g>";
                        $body .= $rollEntry;
                    }

                }
                $vbHeight =  ($gap /2) + (($rowCount + 1) * ($gap + 1200)) * $scale;
                if (!is_null($title)) {
                    $vbHeight += 100;
                }
                if (!is_null($background)) {
                    $body = apply_tincture($background, "<path d=\"m0 0 h1000 v$vbHeight h-1000 z\"/>", $layout) . $body;
                }
                $options['shape'] = 'roll';
                $options['aspectRatio'] = $vbHeight / 1000;
                break;
            case blazonML::E_SHIELD:
                $body .= makeshield($node, $layout);
                $layout = ShieldLayout::from_options($options);
                // options may have changed in makeShield
                $outline = getShape($layout);
                $effect = $options['effect'];
                if (!array_key_exists('nomask', $options)) {
                    $clipShield = add_def('mask', '<path fill="#FFFFFF" d="' . $outline . '" />');
                    $body = '<g mask="url(#' . $clipShield . ')">' . $body . '</g>';
                }
                if ($options['effect'] == 'inked')
                    $body .= '<path fill="none" stroke="#000000" stroke-width="4" d="' . $outline . '" />';
                foreach ($node->childNodes as $childNode) {
                    if ($childNode->nodeName == blazonML::E_ACHIEVEMENT) {
                        $partsList[] = getAchievementParts($childNode, $layout);
                    }
                }
                break;
        }
    }
    if (!array_key_exists('nomask', $options)) {
        if ($options['effect'] == 'shiny') { // this only gets applied to the escutcheon
            $body .= add_def();
            $body = getEffect($effect, 'below') . '<g ' . getEffect($effect, 'filter') . '>' . $body . getEffect($effect, 'above') . '</g>';
        }
        // Add outline of shield shape, only if required
        if ($options['palette'] == 'outline' || $options['asFile'] == 'printable') {
            $outline = getShape($layout);
            $body .= '<path fill="none" stroke="' . rgb('gray') . '" stroke-width="4" d="' . $outline . '" />';
        }
        if ($layout->shape == 'flag') {
            if (count($partsList)) $extraSpace = 120; // should really check for fringe being present
        }
    }
    if (count($partsList)) {
        foreach ($partsList as $parts) {
            $body = addParts($body, $parts, $layout);
        }
    }
    if ($effect != 'shiny') { // other effects are applied everywhere
        $body .= add_def();
        $body = getEffect($effect, 'below') . '<g ' . getEffect($effect, 'filter') . '>' . $body . getEffect($effect, 'above') . '</g>';
    }

    if (is_null($vbHeight)) {
        $vbHeight = 1200;
        if ($options['shape'] == 'flag') { // need to scale tall flags to fit
            if ($options['aspectRatio'] > 1.18) {
                // $shrinkFactor = 1.18 / $options['aspectRatio'];
                // $shiftFactor = (1000 *  (1-$shrinkFactor)) / 2;
                // $body = "<g transform=\"translate($shiftFactor, 0) scale($shrinkFactor)\">$body</g>";
                $svgHeight = $svgWidth * $options['aspectRatio'];
                $vbHeight = 1000 * $options['aspectRatio'];
            } elseif ($options['aspectRatio'] < 1.22) {
                $svgHeight = $svgWidth * $options['aspectRatio'];
                $vbHeight = 1000 * $options['aspectRatio'];
            }
        }
    }

    $margin = $options['margin'];
    if ($margin > 0) {
        $shrink = (1 - ($margin * 2 / $svgWidth));
        $body = '<g transform="translate(' . $margin . ',' . $margin . ') scale(' . $shrink . ')">' .
            $body . "</g>\n";
    }

    // Add a background or margin
    if ($options['background'] != 'transparent') {
        $fill = "none";
        switch ($options['background']) {
            case 'black':
                $fill = "#000000";
                break;
            case 'white':
                $filll = "#FFFFFF";
                break;
            case 'grey':
            case 'gray':
                $fill = "#808080";
                break;
            case 'bisque':
                $fill = "#F1E2BE";
                break;
        }
        $body = '<rect x="0" y="0" height="' . $vbHeight . '" width="1000" fill="' . $fill . "\"/>\n" . $body;
    }

    if (array_key_exists('debug', $options) && strpos($options['debug'], 'grid') !== false) {
        $ordinary = [];
        include "ordinaries/grid.inc";
        $body .= '<g fill="#555555">' . $ordinary['body'] . '</g>';
    }
    $extraDefs = '';
    if (array_key_exists('addHatching', $options)) {
        require("hatching.inc");
        $extraDefs = mkHatches($options['addHatching']);
    }
    $header = '<?xml version="1.0" encoding="utf-8" ?>
    <svg version="1.1"
        baseProfile="full"
        xmlns="http://www.w3.org/2000/svg"
        xmlns:xlink="http://www.w3.org/1999/xlink" 
         xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns:cc="http://creativecommons.org/ns#"
         xmlns:dc="http://purl.org/dc/elements/1.1/" 
        preserveAspectRatio="xMidYMid meet" ';
    $header .= 'height="' . $svgHeight . '" width="' . $svgWidth . '"';
    // $header .= ' height="' . $vbHeight . '" width="1000" ';
    $header .= ' viewBox="' . getEffect($effect, 'viewbox' , 1000, $vbHeight ) . ' ">';
    $defs = '<defs>' . getEffect($effect, 'defs') . $extraDefs . $extraHeader . '</defs>';
    // Now insert the metadata
    $messages->addMessage('legal', "Heraldic images provided on this site are for education and information only. The use of heraldic devices is restricted in many countries, independently of copyright. The granting of a Coat of Arms is solely the right of the appropriate Heraldic Authority for your nationality and country of residence.");
    $rdf = preg_replace('/%CREDITS%/', htmlspecialchars($messages->getCredits('text')), $rdfTemplate);
    $timings['svg draw'] = microtime(true);
    $meta = "\n<metadata>\n" . $rdf . "\n</metadata>\n";
    return $header . $meta . $defs . $body . $trailer;
}
